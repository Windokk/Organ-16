#include "alu.hpp"
#include <iostream>

ALU* ALU::instancePtr = nullptr;
std::mutex ALU::mtx;

ALU_Data ALU::GetALU_Data(uint16_t DataRA, uint16_t DataRB, uint8_t ALU_OpCode, bool enable)
{
    ALU_Data ret = {};
    if (!enable) return ret;

    uint32_t temp_result = 0;

    switch (ALU_OpCode){
        case 0:  // ADD
            temp_result = static_cast<uint32_t>(DataRA) + DataRB;
            ret.result = static_cast<uint16_t>(temp_result);
            break;

        case 1:  // SUB
        case 10: // SUB (redundant code)
            temp_result = static_cast<uint32_t>(DataRA) - DataRB;
            ret.result = static_cast<uint16_t>(temp_result);
            break;

        case 2:  // MUL
            temp_result = static_cast<uint32_t>(DataRA) * DataRB;
            ret.result = static_cast<uint16_t>(temp_result);
            break;
        case 3:
            ret.result = (DataRB != 0) ? (DataRA / DataRB) : 0;
            break;
        case 4:
            ret.result = (DataRB != 0) ? (DataRA % DataRB) : 0;
            break;
        case 5:
            ret.result = DataRA & DataRB;
            break;
        case 6:
            ret.result = DataRA | DataRB;
            break;
        case 7:
            ret.result = !(DataRA & DataRB);
            break;
        case 8:
            ret.result = !(DataRA | DataRB);
            break;
        case 9:
            ret.result = DataRA ^ DataRB;
            break;
        case 11:
            ret.result = ~DataRA;
            break;
        default:
            break;
    }

    // Flags
    ret.zero = (ret.result == 0);
    ret.negative = (ret.result & 0x8000) != 0;  // MSB = 1 means negative in signed 16-bit

    uint8_t MSB_RA = DataRA & 0x8000;
    uint8_t MSB_RB = DataRB & 0x8000;
    uint8_t MSB_Result = ret.result & 0x8000;

    // Carry detection (only for ADD and SUB)
    if (ALU_OpCode == 0) { // ADD
        ret.carry = (temp_result > 0xFFFF);
        ret.overflow = ((~(MSB_RA ^ MSB_RB)) & (MSB_RA ^ MSB_Result));
    } 
    else if (ALU_OpCode == 1 || ALU_OpCode == 10) { // SUB
        ret.carry = (DataRA < DataRB);
        ret.overflow = (((MSB_RA ^ MSB_RB)) & (MSB_RA ^ MSB_Result));
    }

    return ret;
}
