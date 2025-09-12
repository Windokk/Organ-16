#pragma once

#include <mutex>
#include <unordered_map>
#include <string>

struct TempIn{
    uint8_t isNxtExt;
    uint8_t containsAddress;
    uint8_t jsr;
    uint8_t spChange;
    uint8_t isCurrExt;
    uint8_t rts;
    uint8_t regIsAddr;
    bool clockSignal;
};

struct TempOut{
    uint8_t isCurrExt;
    uint8_t isCurrAddrBase;
    uint8_t isCurrAddr;
    uint8_t isCurrAddrJsr;
    uint8_t isCurrSpChange;
    uint8_t isCurrJsr;
    uint8_t isCurrRts;
    uint8_t regIsCurrAddr;
};

void UpdateDebugValues(std::unordered_map<std::string, bool> debugValues);

class TemporaryValues{
    private:
        TemporaryValues() {}
        static TemporaryValues* instancePtr;
        static std::mutex mtx;

        

    public:
        TemporaryValues(const TemporaryValues&) = delete;
        TemporaryValues& operator=(const TemporaryValues&) = delete;
        TemporaryValues(TemporaryValues&&) = delete;
        TemporaryValues& operator=(TemporaryValues&&) = delete;

        // Static method to get the TemporaryValues instance
        static TemporaryValues* GetInstance() {
            if (instancePtr == nullptr) {
                std::lock_guard<std::mutex> lock(mtx);
                if (instancePtr == nullptr) {
                    instancePtr = new TemporaryValues();
                }
            }
            return instancePtr;
        }
        
        std::unordered_map<std::string, uint8_t> flipflops = {
            {"CURRENT_IS_EXT", 0},
            {"CURRENT_IS_ADDR_BASE", 0},
            {"CURRENT_IS_ADDR_JSR", 0},
            {"CURRENT_CHANGES_SP", 0},
            {"CURRENTLY_JSR", 0},
            {"CURRENTLY_RTS", 0},
            {"REG_IS_CURR_ADDR", 0},
        };

        TempOut GetValues(){
            TempOut ret = {};
            ret.isCurrAddr = flipflops["CURRENT_IS_ADDR_BASE"] | flipflops["CURRENT_IS_ADDR_JSR"];
            ret.isCurrAddrBase = flipflops["IS_CURR_ADR_BASE"];
            ret.isCurrAddrJsr = flipflops["CURRENT_IS_ADDR_JSR"];
            ret.isCurrExt = flipflops["CURRENT_IS_EXT"];
            ret.isCurrJsr = flipflops["CURRENTLY_JSR"];
            ret.isCurrRts = flipflops["CURRENTLY_RTS"];
            ret.isCurrSpChange = flipflops["CURRENT_CHANGES_SP"];
            ret.regIsCurrAddr = flipflops["REG_IS_CURR_ADDR"];
            return ret;
        };

        TempOut OnClockChange(TempIn in);

        void ProcessFlipflopsAndUpdateDebug(const std::unordered_map<std::string, uint8_t> &flipflops);

        void Reset(){
            flipflops = {
                {"CURRENT_IS_EXT", 0},
                {"CURRENT_IS_ADDR_BASE", 0},
                {"CURRENT_IS_ADDR_JSR", 0},
                {"CURRENT_CHANGES_SP", 0},
                {"CURRENTLY_JSR", 0},
                {"CURRENTLY_RTS", 0},
                {"REG_IS_CURR_ADDR", 0},
            };
        };
};