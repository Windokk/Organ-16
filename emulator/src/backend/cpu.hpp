#pragma once

#include "clock.hpp"
#include "alu/alu.hpp"
#include "registers/registers.hpp"
#include "control_unit/control_unit.hpp"
#include "temp_values/temp_values.hpp"
#include "memory/memory_interface.hpp"

#include <iostream>
#include <QDebug>

void UpdateVisualRAMCurrentAddress(uint16_t oldAddress, uint16_t newAddress);

class CPU{
    private:
        CPU() {}
        static CPU* instancePtr;
        static std::mutex mtx;

        uint16_t oldRAMAddress = 0;
        uint16_t oldRAMvalue = 0;

        const int ticks_per_frame = Clock::GetInstance()->GetFrequency() * 10;

        void Tick();

        CU_Data FetchControlUnitData();

        ALU_Data PerformALUOperations(const CU_Data &controlUnitData);

        TempOut UpdateTemporaryValuesOnClock(const CU_Data &oldControlUnitData, const TempOut &oldTemporaryValues, bool clockSignal);

        RegsOutOnChange UpdateRegistersOnClock(CU_Data oldControlUnitData, const TempOut &oldTemporaryValues, const RegsOut &oldRegsOut, const ALU_Data &oldAluData, uint16_t oldRAM_OUT, bool currentClockSignal, const TempOut &newTempValues);

        void UpdateRAMOnClock(const CU_Data &oldControlUnitData, const TempOut &oldTemporaryValues, const RegsOut &oldRegsOut, bool currentClockSignal, uint16_t oldRBValue, uint16_t oldRAValue);

        RegsOutOnIdle UpdateRegistersOnIdle(const TempOut &newtempValues, uint16_t newRamValue, uint16_t ir0Data, uint16_t ir1Data, bool currentClockSignal);

    public:
        CPU(const CPU&) = delete;
        CPU& operator=(const CPU&) = delete;
        CPU(CPU&&) = delete;
        CPU& operator=(CPU&&) = delete;

        // Static method to get the CPU instance
        static CPU* GetInstance() {
            if (instancePtr == nullptr) {
                std::lock_guard<std::mutex> lock(mtx);
                if (instancePtr == nullptr) {
                    instancePtr = new CPU();
                }
            }
            return instancePtr;
        }

        void RunFrame();

        void Init();

        void Reset();
};