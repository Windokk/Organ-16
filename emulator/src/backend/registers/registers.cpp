#include "registers.hpp"

#include "../alu/alu.hpp"

RegisterFile* RegisterFile::instancePtr = nullptr;
std::mutex RegisterFile::mtx;

RegsOutOnChange RegisterFile::OnClockChange(RegsInOnChange in)
{
    RegsOutOnChange ret = {};

    // --- General-purpose registers ---
    if(!in.gpClock){
        if(in.gpRegWrite){
            RegisterName reg = static_cast<RegisterName>(in.gpRegToWrite);
            gpRegs.at(reg).updateValue(in.gpData, false);
        }
    }


    // --- Program counter ---
    if(in.pcClock){
        specRegs.at(PC).updateValue(in.writeToPC ? in.pcData : specRegs.at(PC).get() + 1, false);
    }

    // --- Stack pointer ---
    if(in.spClock){
        specRegs.at(SP).updateValue(in.spPop ? specRegs.at(SP).get() + 1 : specRegs.at(SP).get() - 1, false);
    }

    // --- Flags register ---

    uint8_t flagsValue = 
        (in.zero     << 0) |
        (in.negative << 1) |
        (in.carry    << 2) |
        (in.overflow << 3);

    if(in.flagsClock && in.flagsWrite){
        flagReg.updateValue(flagsValue, false);
    }

    // --- Ram Address Register

    if(in.ramAddrClock){
        specRegs.at(RAM_ADDRESS).updateValue(instructionsRegs.at(IR1).get(), false);
    }

    ret.RA = gpRegs.at(static_cast<RegisterName>(in.raRead)).get();
    ret.RB = gpRegs.at(static_cast<RegisterName>(in.rbRead)).get();
    ret.PC = specRegs.at(PC).get();
    ret.SP = specRegs.at(SP).get();
    ret.FLAGS = flagReg.get();
    ret.RAM_ADDRESS = specRegs.at(RAM_ADDRESS).get();

    return ret;
}

RegsOutOnIdle RegisterFile::OnClockIdle(RegsInOnIdle in)
{
    RegsOutOnIdle ret = {};

    if(!in.ir0Clock && in.ir0Write){
        instructionsRegs.at(IR0).updateValue(in.ir0Data, false);
    }

    if(in.ir1Clock){
        instructionsRegs.at(IR1).updateValue(in.ir1Data, false);
    }

    ret.IR0 = instructionsRegs.at(IR0).get();
    ret.IR1 = instructionsRegs.at(IR1).get();

    return ret;
}
