#include "control_unit.hpp"

#include "../registers/registers.hpp"

#include <sstream>

ControlUnit* ControlUnit::instancePtr = nullptr;
std::mutex ControlUnit::mtx;

CU_Data ControlUnit::GetCU_Data(uint16_t IR_0, uint8_t FlagsData)
{
    CU_Data ret = {0};  // Zero-initialize everything

    const uint8_t OpCode = (IR_0 >> 13) & 0b111;
    const uint8_t subOpCode = (IR_0 >> 9) & 0b1111;
    const uint16_t OpData = IR_0 & 0x1FF;

    // --- ALU DATA Logic ---
    if(OpCode < 2){
        if(OpCode == 0){
            switch(subOpCode){
                case 0:
                    ret.ALU_DATA = 16;
                    break;
                case 1:
                    ret.ALU_DATA = 17;
                    break;
                case 2:
                    ret.ALU_DATA = 18;
                    break;
                case 3:
                    ret.ALU_DATA = 19;
                    break;
                case 4:
                    ret.ALU_DATA = 20;
                    break;
                case 5:
                    ret.ALU_DATA = 21;
                    break;
                case 6:
                    ret.ALU_DATA = 22;
                    break;
                case 7:
                    ret.ALU_DATA = 23;
                    break;
                case 8:
                    ret.ALU_DATA = 24;
                    break;
                case 9:
                    ret.ALU_DATA = 25;
                    break;
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                    ret.ALU_DATA = 0;
                    break;
            }
        }
        if(OpCode == 1){
            if(subOpCode == 10){
                ret.ALU_DATA = 26;
            }
            else if(subOpCode == 11){
                ret.ALU_DATA = 27;
            }
            else{
                ret.ALU_DATA = 0;
            }
        }
    }
    else{
        ret.ALU_DATA = 0;
    }

    // --- regWrite Logic ---
    ret.regWrite = 0;
    if(OpCode == 5 && subOpCode == 1){
        ret.regWrite = 1;
    }
    if(OpCode == 3 && subOpCode == 0){
        ret.regWrite = 1;
    }
    if(OpCode == 2){
        ret.regWrite = 1;
    }
    if(OpCode == 1 && subOpCode == 11){
        ret.regWrite = 1;
    }
    if(OpCode == 0){
        ret.regWrite = 1;
    }

    // --- Register fields ---
    ret.dstR  = (OpData >> 6) & 0b111;
    ret.srcRA = (OpData >> 3) & 0b111;
    ret.srcRB = OpData & 0b111;

    // --- isNxtExt Logic ---
    ret.isNxtExt =
        (OpCode == 0b010) ||
        (OpCode == 0b100) ||
        (OpCode == 0b011 && subOpCode < 2) ||
        (OpCode == 0b101 && subOpCode == 1);

    // --- flagsWrite ---
    ret.flagsWrite = (OpCode == 0b001 && subOpCode == 10);

    // --- memWrite ---
    ret.memWrite = (OpCode == 0b011 && (subOpCode == 1 || subOpCode == 2));

    // --- memToReg ---
    ret.memToReg = (OpCode == 0b011 && subOpCode == 0);

    // --- Flags Mux Logic ---
    const bool XOR3_1 = (FlagsData & 0b1000) ^ (FlagsData & 0b0010);

    const uint8_t bit0 = FlagsData & 0b0001;
    const uint8_t bit1 = FlagsData & 0b0010;
    const uint8_t bit2 = FlagsData & 0b0100;
    const uint8_t bit3 = FlagsData & 0b1000;

    bool flagsMux = false;
    switch (subOpCode) {
        case 0:   flagsMux = true; break;
        case 1:   flagsMux = bit0; break;
        case 2:   flagsMux = !bit0; break;
        case 3:   flagsMux = bit1; break;
        case 4:   flagsMux = bit0 | bit1; break;
        case 5:   flagsMux = ~(bit0 ^ bit2); break;
        case 6:   flagsMux = !bit2; break;
        case 7:   flagsMux = XOR3_1; break;
        case 8:   flagsMux = bit0 | XOR3_1; break;
        case 9:   flagsMux = !bit0 & !XOR3_1; break;
        case 10:  flagsMux = !XOR3_1; break;
        case 11:  flagsMux = true; break;
        default:  flagsMux = false; break;
    }

    // --- containsAddress ---
    if (OpCode == 0b011)
        ret.containsAddress = 1;
    else if (OpCode == 0b100)
        ret.containsAddress = static_cast<uint8_t>(flagsMux);
    else
        ret.containsAddress = 0;

    // --- loadPC ---
    ret.loadPC = (OpCode == 0b100);

    // --- spPop ---
    ret.spPop = (OpCode == 0b100 && subOpCode == 12) ||
                (OpCode == 0b101 && subOpCode == 1);

    // --- spChange ---
    ret.spChange = (OpCode == 0b100 && (subOpCode == 11 || subOpCode == 12)) ||
                   (OpCode == 0b101);

    // --- jsr ---
    ret.jsr = (OpCode == 0b100 && subOpCode == 11);

    // --- rts ---
    ret.rts = (OpCode == 0b100 && subOpCode == 12);

    // --- regIsAddress ---
    ret.regIsAddress = (OpCode == 3 && subOpCode == 2);

    // --- HLT ---
    ret.HLT = (OpCode == 0b111);

    return ret;
} 