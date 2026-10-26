#include "screenshot.hpp"

#include <array>

namespace {

constexpr int JPEG_QUALITY = 95;

}

QImage RenderExportImage(const ScreenPixels& pixels)
{
    std::array<uint32_t, SCREEN_PIXELS> colors;
    for (size_t i = 0; i < SCREEN_PIXELS; ++i)
        colors[i] = 0xFF000000u | rgb565ToRgb(pixels[i]);

    std::array<int, EXPORT_SIZE> columns;
    for (int x = 0; x < EXPORT_SIZE; ++x)
        columns[x] = nearestSource(x);

    QImage image(EXPORT_SIZE, EXPORT_SIZE, QImage::Format_RGB32);
    for (int y = 0; y < EXPORT_SIZE; ++y) {
        uint32_t* line = reinterpret_cast<uint32_t*>(image.scanLine(y));
        const uint32_t* source = colors.data() + static_cast<size_t>(nearestSource(y)) * SCREEN_WIDTH;
        for (int x = 0; x < EXPORT_SIZE; ++x)
            line[x] = source[columns[x]];
    }
    return image;
}

bool SaveScreenshotJpeg(const ScreenPixels& pixels, const QString& path)
{
    return RenderExportImage(pixels).save(path, "JPEG", JPEG_QUALITY);
}
