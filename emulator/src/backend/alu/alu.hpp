#pragma once

#include <mutex>

struct ALU_Data{
    uint16_t result = 0;
    bool zero = false;
    bool negative = false;
    bool carry = false;
    bool overflow = false;
};

class ALU{
    private:
        ALU() {}
        static ALU* instancePtr;
        static std::mutex mtx;

        
    public:
        ALU(const ALU&) = delete;
        ALU& operator=(const ALU&) = delete;
        ALU(ALU&&) = delete;
        ALU& operator=(ALU&&) = delete;

        // Static method to get the ALU instance
        static ALU* GetInstance() {
            if (instancePtr == nullptr) {
                std::lock_guard<std::mutex> lock(mtx);
                if (instancePtr == nullptr) {
                    instancePtr = new ALU();
                }
            }
            return instancePtr;
        }

        ALU_Data GetALU_Data(uint16_t DataRA, uint16_t DataRB, uint8_t ALU_OpCode, bool enable);

};