#include "decode.hpp"

namespace {

bool jumpCondition(unsigned subOpCode, unsigned flags) {
    const bool z = flags & 1;
    const bool n = flags & 2;
    const bool c = flags & 4;
    const bool o = flags & 8;
    switch (subOpCode) {
        case 0:  return true;
        case 1:  return z;
        case 2:  return !z;
        case 3:  return c;
        case 4:  return z || c;
        case 5:  return !(z || c);
        case 6:  return !c;
        case 7:  return n != o;
        case 8:  return z || (n != o);
        case 9:  return !z && (n == o);
        case 10: return n == o;
        case 11: return true;
        default: return false;
    }
}

constexpr std::array<int8_t, 16> aluOpsByThreeRegSub = {
    -1, ALU_SUB, ALU_MUL, ALU_DIV, ALU_MOD, ALU_AND, ALU_OR, ALU_NAND,
    ALU_NOR, ALU_XOR, ALU_ADD, ALU_SHL, ALU_SHR, ALU_XNOR, ALU_MIN, ALU_MAX
};

constexpr std::array<int8_t, 16> aluOpsByTwoRegSub = {
    ALU_SIN, ALU_COS, ALU_TAN, ALU_SQRT, ALU_POW, ALU_ABS, -1, -1,
    -1, -1, ALU_CMP, ALU_NOT, -1, -1, -1, -1
};

Decoded decode(unsigned opCode, unsigned sub) {
    Decoded d;

    int aluOp = -1;
    if (opCode == 0)
        aluOp = aluOpsByThreeRegSub[sub];
    else if (opCode == 1)
        aluOp = aluOpsByTwoRegSub[sub];
    else if (opCode == 2 && sub == 1)
        aluOp = ALU_MOV;

    if (aluOp >= 0) {
        d.aluEnabled = true;
        d.aluOp = static_cast<uint8_t>(aluOp);
    }

    d.regWrite = (d.aluEnabled && d.aluOp != ALU_CMP) || (opCode == 2) ||
                 (opCode == 3 && (sub == 0 || sub == 3)) ||
                 (opCode == 5 && sub == 1) ||
                 (opCode == 7 && ((sub >= 1 && sub <= 3) || sub == 7));

    d.nextExt = (opCode == 2 && sub == 0) || (opCode == 4) ||
                (opCode == 3 && sub < 2) ||
                (opCode == 5 && sub == 1);

    d.flagsWrite = d.aluEnabled && d.aluOp == ALU_CMP;
    d.memWrite = (opCode == 3 && (sub == 1 || sub == 2));
    d.loadPC = (opCode == 4);
    d.spPop = (opCode == 4 && sub == 12) || (opCode == 5 && sub == 1);
    d.spChange = (opCode == 4 && (sub == 11 || sub == 12)) || (opCode == 5);
    d.jsr = (opCode == 4 && sub == 11);
    d.rts = (opCode == 4 && sub == 12);
    d.regIsAddress = (opCode == 3 && (sub == 2 || sub == 3));
    d.hlt = (opCode == 7 && sub == 0);
    d.useIn = (opCode == 7 && ((sub >= 1 && sub <= 3) || sub == 7));
    d.useOut = (opCode == 7 && sub >= 4);

    if (opCode == 3 || (opCode == 5 && sub == 1))
        d.addressMode = ADDR_ALWAYS;
    else if (opCode == 4)
        d.addressMode = ADDR_IF_JUMP_TAKEN;

    if (opCode == 7 && sub >= 1 && sub <= 6)
        d.ioPort = (sub - 1) % 3;
    else if (opCode == 7 && sub == 7)
        d.ioPort = RANDOM_PORT;

    return d;
}

std::array<Decoded, 128> buildDecodeTable() {
    std::array<Decoded, 128> table;
    for (unsigned i = 0; i < 128; ++i)
        table[i] = decode(i >> 4, i & 15);
    return table;
}

std::array<uint16_t, 16> buildJumpMasks() {
    std::array<uint16_t, 16> masks{};
    for (unsigned sub = 0; sub < 16; ++sub)
        for (unsigned flags = 0; flags < 16; ++flags)
            if (jumpCondition(sub, flags))
                masks[sub] |= 1u << flags;
    return masks;
}

}

const std::array<Decoded, 128> decodeTable = buildDecodeTable();
const std::array<uint16_t, 16> jumpMasks = buildJumpMasks();
