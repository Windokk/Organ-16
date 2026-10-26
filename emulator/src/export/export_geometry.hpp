#pragma once

#include <array>
#include <cstdint>

#include "core/core.hpp"

constexpr int EXPORT_SIZE = 800;

using ScreenPixels = std::array<uint16_t, SCREEN_PIXELS>;

constexpr int nearestSource(int destination) {
    return ((2 * destination + 1) * static_cast<int>(SCREEN_WIDTH)) / (2 * EXPORT_SIZE);
}

constexpr uint32_t rgb565ToRgb(uint16_t value) {
    const uint32_t red = (((value >> 11) & 0x1F) * 255) / 31;
    const uint32_t green = (((value >> 5) & 0x3F) * 255) / 63;
    const uint32_t blue = ((value & 0x1F) * 255) / 31;
    return (red << 16) | (green << 8) | blue;
}
