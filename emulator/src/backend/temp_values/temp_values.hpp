#pragma once

#include <mutex>
#include <unordered_map>
#include <string>

struct TempIn{
    uint8_t isNxtExt = 0;
    uint8_t containsAddress = 0;
    uint8_t jsr = 0;
    uint8_t spChange = 0;
    uint8_t isCurrExt = 0;
    uint8_t rts = 0;
    uint8_t regIsAddr = 0;
    bool clockSignal = 0;
};

struct TempOut{
    uint8_t isCurrExt = 0;
    uint8_t isCurrAddrBase = 0;
    uint8_t isCurrAddr = 0;
    uint8_t isCurrAddrJsr = 0;
    uint8_t isCurrSpChange = 0;
    uint8_t isCurrJsr = 0;
    uint8_t isCurrRts = 0;
    uint8_t regIsCurrAddr = 0;
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