#pragma once

#include <array>
#include <cstdint>

struct Decoded {
    uint8_t aluData = 0;
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
    bool hlt = false;
    bool useIn = false;
    bool useOut = false;
};

enum AddressMode : uint8_t { ADDR_NEVER = 0, ADDR_ALWAYS = 1, ADDR_IF_JUMP_TAKEN = 2 };

extern const std::array<Decoded, 128> decodeTable;
extern const std::array<uint16_t, 16> jumpMasks;

inline bool jumpTaken(unsigned subOpCode, unsigned flags) {
    return (jumpMasks[subOpCode] >> flags) & 1;
}
