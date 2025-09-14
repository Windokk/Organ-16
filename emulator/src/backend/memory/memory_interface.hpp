#pragma once

#include <mutex>

#include "ram.hpp"

struct MI_Data{
    uint16_t RAM_ADDRESS;
    bool writeToRAM;
    bool RAM_Clock;
    uint16_t RAM_DATA;
};

struct CU_Data;
struct RegsOut;
struct TempOut;

class MemoryInterface{
    private:
        MemoryInterface() {}
        static MemoryInterface* instancePtr;
        static std::mutex mtx;

        bool writeToRAMFlipFlop = false;
        
        uint16_t RAM_OUT = 0;

    public:
        MemoryInterface(const MemoryInterface&) = delete;
        MemoryInterface& operator=(const MemoryInterface&) = delete;
        MemoryInterface(MemoryInterface&&) = delete;
        MemoryInterface& operator=(MemoryInterface&&) = delete;

        // Static method to get the MemoryInterface instance
        static MemoryInterface* GetInstance() {
            if (instancePtr == nullptr) {
                std::lock_guard<std::mutex> lock(mtx);
                if (instancePtr == nullptr) {
                    instancePtr = new MemoryInterface();
                }
            }
            return instancePtr;
        }

        // Returns the RAM Out value on clock change
        void OnClockChange(MI_Data data){
            if(data.writeToRAM)
                RAM::GetInstance()->Write(data.RAM_ADDRESS, data.RAM_DATA, data.RAM_Clock);
        }

        void Reset(){
            writeToRAMFlipFlop = false;
            RAM::GetInstance()->Reset();
        }
        
       MI_Data GetMI_Data(CU_Data oldCUData, RegsOut oldRegsOut, TempOut oldTempValues, bool currentClockSignal, bool memWrite, uint16_t oldRB, uint16_t oldRA, bool regWrite);
};