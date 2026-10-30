#pragma once

#include <array>
#include <cstdint>

enum AluOp : uint8_t {
    ALU_ADD, ALU_SUB, ALU_MUL, ALU_DIV, ALU_MOD, ALU_AND, ALU_OR, ALU_NAND, ALU_NOR, ALU_XOR,
    ALU_XNOR, ALU_SHL, ALU_SHR, ALU_MIN, ALU_MAX, ALU_NOT, ALU_CMP,
    ALU_SIN, ALU_COS, ALU_TAN, ALU_SQRT, ALU_POW, ALU_ABS, ALU_MOV, ALU_INC, ALU_DEC
};

struct Decoded {
    uint8_t aluOp = 0;
    bool aluEnabled = false;
    bool aluUsesExt = false;
    uint8_t ioPort = 0;
    uint8_t addressMode = 0;
    bool regWrite = false;
    bool nextExt = false;
    bool flagsWrite = false;
    bool memWrite = false;
    bool loadPC = false;
    bool spPop = false;
    bool spChange = false;
    bool jsr = false;
    bool rts = false;
    bool regIsAddress = false;
    bool postInc = false;
    bool hlt = false;
    bool useIn = false;
    bool useOut = false;
};

constexpr uint8_t RANDOM_PORT = 3;

enum AddressMode : uint8_t { ADDR_NEVER = 0, ADDR_ALWAYS = 1, ADDR_IF_JUMP_TAKEN = 2 };

extern const std::array<Decoded, 128> decodeTable;
extern const std::array<uint16_t, 16> jumpMasks;

inline bool jumpTaken(unsigned subOpCode, unsigned flags) {
    return (jumpMasks[subOpCode] >> flags) & 1;
}
