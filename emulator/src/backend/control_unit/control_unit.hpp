#pragma once

#include <mutex>


struct CU_Data {
    uint8_t ALU_DATA = 0;
    uint8_t regWrite = 0;
    uint8_t srcRA = 0;
    uint8_t srcRB = 0;
    uint8_t dstR = 0;
    uint8_t isNxtExt = 0;
    uint8_t flagsWrite = 0;
    uint8_t memWrite = 0;
    uint8_t memToReg = 0;
    uint8_t containsAddress = 0;
    uint8_t loadPC = 0;
    uint8_t spPop = 0;
    uint8_t spChange = 0;
    uint8_t jsr = 0;
    uint8_t rts = 0;
    uint8_t regIsAddress = 0;
    uint8_t HLT = 0;
};

class ControlUnit{
    private:
        ControlUnit() {}
        static ControlUnit* instancePtr;
        static std::mutex mtx;
        
    public:
        ControlUnit(const ControlUnit&) = delete;
        ControlUnit& operator=(const ControlUnit&) = delete;
        ControlUnit(ControlUnit&&) = delete;
        ControlUnit& operator=(ControlUnit&&) = delete;

        // Static method to get the ControlUnit instance
        static ControlUnit* GetInstance() {
            if (instancePtr == nullptr) {
                std::lock_guard<std::mutex> lock(mtx);
                if (instancePtr == nullptr) {
                    instancePtr = new ControlUnit();
                }
            }
            return instancePtr;
        }

        CU_Data GetCU_Data(uint16_t IR_0, uint8_t FlagsData);
};