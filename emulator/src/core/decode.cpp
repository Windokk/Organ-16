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

Decoded decode(unsigned opCode, unsigned sub) {
    Decoded d;

    if (opCode == 0 && sub <= 9)
        d.aluData = 16 + sub;
    else if (opCode == 1 && sub == 10)
        d.aluData = 26;
    else if (opCode == 1 && sub == 11)
        d.aluData = 27;

    d.regWrite = (opCode == 0) || (opCode == 2) ||
                 (opCode == 1 && sub == 11) ||
                 (opCode == 3 && (sub == 0 || sub == 3)) ||
                 (opCode == 5 && sub == 1) ||
                 (opCode == 7 && sub >= 1 && sub <= 3);

    d.nextExt = (opCode == 2) || (opCode == 4) ||
                (opCode == 3 && sub < 2) ||
                (opCode == 5 && sub == 1);

    d.flagsWrite = (opCode == 1 && sub == 10);
    d.memWrite = (opCode == 3 && (sub == 1 || sub == 2));
    d.loadPC = (opCode == 4);
    d.spPop = (opCode == 4 && sub == 12) || (opCode == 5 && sub == 1);
    d.spChange = (opCode == 4 && (sub == 11 || sub == 12)) || (opCode == 5);
    d.jsr = (opCode == 4 && sub == 11);
    d.rts = (opCode == 4 && sub == 12);
    d.regIsAddress = (opCode == 3 && (sub == 2 || sub == 3));
    d.hlt = (opCode == 7 && sub == 0);
    d.useIn = (opCode == 7 && sub >= 1 && sub <= 3);
    d.useOut = (opCode == 7 && sub >= 4);

    if (opCode == 3 || (opCode == 5 && sub == 1))
        d.addressMode = ADDR_ALWAYS;
    else if (opCode == 4)
        d.addressMode = ADDR_IF_JUMP_TAKEN;

    if (opCode == 7 && sub >= 1 && sub <= 6)
        d.ioPort = (sub - 1) % 3;

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
