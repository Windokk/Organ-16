#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "export/export_geometry.hpp"

class GifRecorder {
    public:
        GifRecorder();
        ~GifRecorder();

        GifRecorder(const GifRecorder&) = delete;
        GifRecorder& operator=(const GifRecorder&) = delete;

        bool start(const std::string& path);
        bool addFrame(const ScreenPixels& pixels);
        bool stop();

        bool recording() const { return out.is_open(); }

    private:
        using Clock = std::chrono::steady_clock;

        struct HashSlot {
            uint32_t key = 0;
            uint32_t generation = 0;
            uint16_t code = 0;
        };

        void writeHeader();
        void writeFrame(const ScreenPixels& pixels, int delayCentiseconds);
        void flushPending(Clock::time_point now);
        int buildPalette(const ScreenPixels& pixels);
        void encodeFrame(int bitsPerPixel);
        void emitCode(uint32_t code);
        void writeBlocks(const std::vector<uint8_t>& data);

        std::ofstream out;
        ScreenPixels pending{};
        bool hasPending = false;
        Clock::time_point startTime;
        Clock::time_point pendingSince;
        int64_t writtenCentiseconds = 0;

        std::vector<int16_t> colorIndex;
        std::vector<uint16_t> paletteColors;
        std::array<uint8_t, SCREEN_PIXELS> indices{};
        std::array<uint8_t, EXPORT_SIZE> scaledRow{};
        std::vector<HashSlot> hash;
        uint32_t generation = 0;
        std::vector<uint8_t> packed;
        uint32_t bitBuffer = 0;
        int bitCount = 0;
        int codeSize = 0;
};
