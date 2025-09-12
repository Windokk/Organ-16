#pragma once

#include <mutex>
#include <array>
#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <utility>

#include <QColor>

void ResetVisualRAM();

std::pair<int, int> GetScreenDim();
void SetScreenPixel(int x, int y, QColor color);

static const size_t ADDRESS_SPACE = 65536;

class RAM{
    private:
        static RAM* instancePtr;
        static std::mutex mtx;

        std::array<uint16_t, ADDRESS_SPACE> memory{};

        RAM() {
            memory.fill(0);
        }

    public:

        RAM(const RAM&) = delete;
        RAM& operator=(const RAM&) = delete;
        RAM(RAM&&) = delete;
        RAM& operator=(RAM&&) = delete;

        // Static method to get the RAM instance
        static RAM* GetInstance() {
            if (instancePtr == nullptr) {
                std::lock_guard<std::mutex> lock(mtx);
                if (instancePtr == nullptr) {
                    instancePtr = new RAM();
                }
            }
            return instancePtr;
        }

        uint16_t Read(uint16_t address);

        void Write(uint16_t address, uint16_t data, bool clockSignal);

        void Reset();

        void Load(std::vector<uint16_t> vec){
            std::copy_n(vec.begin(), ADDRESS_SPACE, memory.begin());
        }

};