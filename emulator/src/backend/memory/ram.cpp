#include "ram.hpp"

RAM* RAM::instancePtr = nullptr;
std::mutex RAM::mtx;


uint16_t RAM::Read(uint16_t address)
 {
    if (address >= ADDRESS_SPACE){
        std::stringstream ss;
        ss << "0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << address;
        throw std::out_of_range("trying to read an invalid memory address : " + ss.str());
    }
    return memory[address];
}

void RAM::Write(uint16_t address, uint16_t data, bool clockSignal)
{
    if (address >= ADDRESS_SPACE)
        throw std::out_of_range("trying to write to an invalid memory address");
    if(clockSignal){
        memory[address] = data;
        if (address >= 0x8000 && address < 0xC000) {

            uint16_t relativeAddress = address - 0x8000;

            // Get screen dimensions
            int widthInPx = GetScreenDim().first;
            int heightInPx = GetScreenDim().second;

            // Calculate x and y coordinates on the screen
            int x = relativeAddress % widthInPx;
            int y = relativeAddress / widthInPx;

            // Extracting 5-6-5 RGB components
            uint8_t r5 = (data >> 11) & 0x1F;  // bits 15-11
            uint8_t g6 = (data >> 5) & 0x3F;   // bits 10-5
            uint8_t b5 = data & 0x1F;          // bits 4-0

            int r = (r5 * 255) / 31;
            int g = (g6 * 255) / 63;
            int b = (b5 * 255) / 31;

            QColor color(r, g, b, 255);

            SetScreenPixel(x, y, color);
        }
    }
}

void RAM::Reset() {
    memory.fill(0);
    ResetVisualRAM();
}