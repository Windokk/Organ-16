#include "memory_interface.hpp"

#include "../control_unit/control_unit.hpp"
#include "../registers/registers.hpp"
#include "../temp_values/temp_values.hpp"

MemoryInterface* MemoryInterface::instancePtr = nullptr;
std::mutex MemoryInterface::mtx;

MI_Data MemoryInterface::GetMI_Data(CU_Data oldCUData, RegsOut oldRegsOut, TempOut oldTempValues, bool currentClockSignal, bool memWrite, uint16_t oldRB, uint16_t oldRA)
{
    MI_Data ret = {};
    
    if((!oldTempValues.isCurrJsr & oldTempValues.isCurrAddr & oldTempValues.isCurrExt & !oldCUData.loadPC) | oldTempValues.isCurrSpChange | oldTempValues.regIsCurrAddr){
        if(oldTempValues.isCurrSpChange){
            ret.RAM_ADDRESS = oldRegsOut.SP;
        }
        else{
            if(oldTempValues.regIsCurrAddr){
                ret.RAM_ADDRESS = oldRB;
            }
            else{
                ret.RAM_ADDRESS = oldRegsOut.RAM_ADDRESS;
            }
        }
    }
    else{
        ret.RAM_ADDRESS = oldRegsOut.PC;
    }

    if(!currentClockSignal){
        if(oldTempValues.isCurrExt){
            writeToRAMFlipFlop = 0;
        }
        else{
            writeToRAMFlipFlop = !writeToRAMFlipFlop & (memWrite | oldTempValues.isCurrJsr);
        }
    }

    ret.writeToRAM = writeToRAMFlipFlop | oldTempValues.regIsCurrAddr;
    ret.RAM_Clock = oldTempValues.isCurrSpChange ? !currentClockSignal : currentClockSignal;
    ret.RAM_DATA = oldTempValues.isCurrJsr ? oldRegsOut.PC + 2 : oldRA;

    return ret;
}