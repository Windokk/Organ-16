#include "temp_values.hpp"
#include <iostream>

TemporaryValues* TemporaryValues::instancePtr = nullptr;
std::mutex TemporaryValues::mtx;

TempOut TemporaryValues::OnClockChange(TempIn in)
{
    TempOut ret = {};

    if(in.clockSignal){
        if(flipflops.at("CURRENTLY_JSR")){
            flipflops.at("CURRENT_IS_EXT") = 1;
        }
        else if(in.isNxtExt){
            flipflops.at("CURRENT_IS_EXT") = 1;
        }
        else{
            flipflops.at("CURRENT_IS_EXT") = 0;
        }
    
        if(in.containsAddress){
            flipflops.at("CURRENT_IS_ADDR_JSR") = 1;
        }
        else{
            flipflops.at("CURRENT_IS_ADDR_JSR") = 0;
        }

        if(in.spChange){
            flipflops.at("CURRENT_CHANGES_SP") = 1;
        }
        else{
            flipflops.at("CURRENT_CHANGES_SP") = 0;
        }

        if(in.jsr && !in.isCurrExt){
            flipflops.at("CURRENTLY_JSR") = 1;
        }
        else{
            flipflops.at("CURRENTLY_JSR") = 0;
        }
    
        if(in.rts){
            flipflops.at("CURRENTLY_RTS") = 1;
        }
        else{
            flipflops.at("CURRENTLY_RTS") = 0;
        }

        if(in.regIsAddr){
            flipflops.at("REG_IS_CURR_ADDR") = 1;
        }
        else{
            flipflops.at("REG_IS_CURR_ADDR") = 0;
        }

    }
    else{
        if(in.containsAddress && !in.jsr){
            flipflops.at("CURRENT_IS_ADDR_BASE") = 1;
        }
        else{
            flipflops.at("CURRENT_IS_ADDR_BASE") = 0;
        }
    }

    ret.isCurrAddr = flipflops.at("CURRENT_IS_ADDR_BASE") | flipflops.at("CURRENT_IS_ADDR_JSR");
    ret.isCurrAddrBase = flipflops.at("CURRENT_IS_ADDR_BASE");
    ret.isCurrAddrJsr = flipflops.at("CURRENT_IS_ADDR_JSR");
    ret.isCurrExt = flipflops.at("CURRENT_IS_EXT");
    ret.isCurrJsr = flipflops.at("CURRENTLY_JSR");
    ret.isCurrRts = flipflops.at("CURRENTLY_RTS");
    ret.isCurrSpChange = flipflops.at("CURRENT_CHANGES_SP");
    ret.regIsCurrAddr = flipflops.at("REG_IS_CURR_ADDR");

    ProcessFlipflopsAndUpdateDebug(flipflops);

    return ret;
}

void TemporaryValues::ProcessFlipflopsAndUpdateDebug(const std::unordered_map<std::string, uint8_t>& flipflops) {
    std::unordered_map<std::string, bool> flipflopsValues = {
        {"IsCurrExt", static_cast<bool>(flipflops.at("CURRENT_IS_EXT"))},
        {"IsCurrAddrBase", static_cast<bool>(flipflops.at("CURRENT_IS_ADDR_BASE"))},
        {"IsCurrAddr", static_cast<bool>(flipflops.at("CURRENT_IS_ADDR_BASE")) || static_cast<bool>(flipflops.at("CURRENT_IS_ADDR_JSR"))},
        {"IsCurrAddrJsr", static_cast<bool>(flipflops.at("CURRENT_IS_ADDR_JSR"))},
        {"IsCurrSpChange", static_cast<bool>(flipflops.at("CURRENT_CHANGES_SP"))},
        {"IsCurrJsr", static_cast<bool>(flipflops.at("CURRENTLY_JSR"))},
        {"IsCurrRts", static_cast<bool>(flipflops.at("CURRENTLY_RTS"))},
        {"RegIsCurrAddr", static_cast<bool>(flipflops.at("REG_IS_CURR_ADDR"))}
    };

    UpdateDebugValues(flipflopsValues);
}