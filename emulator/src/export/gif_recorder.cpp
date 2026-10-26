#include "gif_recorder.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr size_t HASH_SIZE = 1 << 13;
constexpr int MAX_CODES = 4096;
constexpr int MIN_DELAY_CENTISECONDS = 2;
constexpr int MAX_DELAY_CENTISECONDS = 65535;

void putWord(std::ofstream& out, uint16_t value)
{
    out.put(static_cast<char>(value & 0xFF));
    out.put(static_cast<char>(value >> 8));
}

uint16_t quantize(uint16_t value, int dropBits)
{
    if (dropBits == 0)
        return value;
    const uint16_t red = ((value >> 11) & 0x1F) >> dropBits << dropBits;
    const uint16_t green = ((value >> 5) & 0x3F) >> dropBits << dropBits;
    const uint16_t blue = (value & 0x1F) >> dropBits << dropBits;
    return static_cast<uint16_t>((red << 11) | (green << 5) | blue);
}

}

GifRecorder::GifRecorder() : colorIndex(65536, -1), hash(HASH_SIZE) {}

GifRecorder::~GifRecorder()
{
    stop();
}

bool GifRecorder::start(const std::string& path)
{
    if (recording())
        return false;
    out.open(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    hasPending = false;
    writtenCentiseconds = 0;
    startTime = Clock::now();
    writeHeader();
    return static_cast<bool>(out);
}

void GifRecorder::writeHeader()
{
    out.write("GIF89a", 6);
    putWord(out, EXPORT_SIZE);
    putWord(out, EXPORT_SIZE);
    out.put(0);
    out.put(0);
    out.put(0);

    out.put(0x21);
    out.put(static_cast<char>(0xFF));
    out.put(11);
    out.write("NETSCAPE2.0", 11);
    out.put(3);
    out.put(1);
    putWord(out, 0);
    out.put(0);
}

bool GifRecorder::addFrame(const ScreenPixels& pixels)
{
    if (!recording())
        return false;

    const Clock::time_point now = Clock::now();
    if (hasPending && pixels == pending)
        return static_cast<bool>(out);

    if (hasPending)
        flushPending(now);
    pending = pixels;
    pendingSince = now;
    hasPending = true;
    return static_cast<bool>(out);
}

void GifRecorder::flushPending(Clock::time_point now)
{
    const double elapsedMs = std::chrono::duration<double, std::milli>(now - startTime).count();
    const int64_t targetCentiseconds = std::llround(elapsedMs / 10.0);
    const int64_t delay = std::clamp<int64_t>(targetCentiseconds - writtenCentiseconds,
                                              MIN_DELAY_CENTISECONDS, MAX_DELAY_CENTISECONDS);
    writeFrame(pending, static_cast<int>(delay));
    writtenCentiseconds += delay;
    hasPending = false;
}

bool GifRecorder::stop()
{
    if (!recording())
        return true;
    if (hasPending)
        flushPending(Clock::now());
    out.put(0x3B);
    out.flush();
    const bool ok = static_cast<bool>(out);
    out.close();
    return ok;
}

int GifRecorder::buildPalette(const ScreenPixels& pixels)
{
    for (uint16_t color : paletteColors)
        colorIndex[color] = -1;
    paletteColors.clear();

    for (int dropBits = 0; dropBits <= 5; ++dropBits) {
        bool fits = true;
        for (size_t i = 0; i < SCREEN_PIXELS; ++i) {
            const uint16_t color = quantize(pixels[i], dropBits);
            int16_t& slot = colorIndex[color];
            if (slot < 0) {
                if (paletteColors.size() == 256) {
                    fits = false;
                    break;
                }
                slot = static_cast<int16_t>(paletteColors.size());
                paletteColors.push_back(color);
            }
            indices[i] = static_cast<uint8_t>(slot);
        }
        if (fits)
            break;
        for (uint16_t color : paletteColors)
            colorIndex[color] = -1;
        paletteColors.clear();
    }

    int bits = 1;
    while ((1u << bits) < paletteColors.size())
        ++bits;
    return bits;
}

void GifRecorder::emitCode(uint32_t code)
{
    bitBuffer |= code << bitCount;
    bitCount += codeSize;
    while (bitCount >= 8) {
        packed.push_back(static_cast<uint8_t>(bitBuffer & 0xFF));
        bitBuffer >>= 8;
        bitCount -= 8;
    }
}

void GifRecorder::encodeFrame(int bitsPerPixel)
{
    const int minCodeSize = std::max(2, bitsPerPixel);
    const uint32_t clearCode = 1u << minCodeSize;
    const uint32_t endCode = clearCode + 1;

    packed.clear();
    bitBuffer = 0;
    bitCount = 0;
    codeSize = minCodeSize + 1;
    uint32_t nextCode = endCode + 1;
    ++generation;
    emitCode(clearCode);

    uint32_t prefix = 0;
    bool first = true;
    int cachedRow = -1;

    for (int y = 0; y < EXPORT_SIZE; ++y) {
        const int sourceRow = nearestSource(y);
        if (sourceRow != cachedRow) {
            cachedRow = sourceRow;
            const uint8_t* source = indices.data() + static_cast<size_t>(sourceRow) * SCREEN_WIDTH;
            for (int x = 0; x < EXPORT_SIZE; ++x)
                scaledRow[x] = source[nearestSource(x)];
        }

        for (int x = 0; x < EXPORT_SIZE; ++x) {
            const uint8_t pixel = scaledRow[x];
            if (first) {
                prefix = pixel;
                first = false;
                continue;
            }

            const uint32_t key = (prefix << 8) | pixel;
            size_t slot = (key * 2654435761u) >> 19;
            bool found = false;
            while (hash[slot].generation == generation) {
                if (hash[slot].key == key) {
                    prefix = hash[slot].code;
                    found = true;
                    break;
                }
                slot = (slot + 1) & (HASH_SIZE - 1);
            }
            if (found)
                continue;

            emitCode(prefix);
            if (nextCode < MAX_CODES) {
                hash[slot] = {key, generation, static_cast<uint16_t>(nextCode)};
                if (nextCode == (1u << codeSize))
                    ++codeSize;
                ++nextCode;
            }
            else {
                emitCode(clearCode);
                codeSize = minCodeSize + 1;
                nextCode = endCode + 1;
                ++generation;
            }
            prefix = pixel;
        }
    }

    emitCode(prefix);
    emitCode(endCode);
    if (bitCount > 0)
        packed.push_back(static_cast<uint8_t>(bitBuffer & 0xFF));
}

void GifRecorder::writeBlocks(const std::vector<uint8_t>& data)
{
    for (size_t offset = 0; offset < data.size(); offset += 255) {
        const size_t length = std::min<size_t>(255, data.size() - offset);
        out.put(static_cast<char>(length));
        out.write(reinterpret_cast<const char*>(data.data() + offset), static_cast<std::streamsize>(length));
    }
    out.put(0);
}

void GifRecorder::writeFrame(const ScreenPixels& pixels, int delayCentiseconds)
{
    const int bits = buildPalette(pixels);
    const int minCodeSize = std::max(2, bits);
    const int tableSize = 1 << minCodeSize;

    out.put(0x21);
    out.put(static_cast<char>(0xF9));
    out.put(4);
    out.put(0x04);
    putWord(out, static_cast<uint16_t>(delayCentiseconds));
    out.put(0);
    out.put(0);

    out.put(0x2C);
    putWord(out, 0);
    putWord(out, 0);
    putWord(out, EXPORT_SIZE);
    putWord(out, EXPORT_SIZE);
    out.put(static_cast<char>(0x80 | (minCodeSize - 1)));

    for (int i = 0; i < tableSize; ++i) {
        const uint32_t rgb = i < static_cast<int>(paletteColors.size()) ? rgb565ToRgb(paletteColors[i]) : 0;
        out.put(static_cast<char>(rgb >> 16));
        out.put(static_cast<char>((rgb >> 8) & 0xFF));
        out.put(static_cast<char>(rgb & 0xFF));
    }

    out.put(static_cast<char>(minCodeSize));
    encodeFrame(bits);
    writeBlocks(packed);
}
