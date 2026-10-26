#include "core.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace {

struct AluResult {
    uint16_t result = 0;
    bool zero = false;
    bool negative = false;
    bool carry = false;
    bool overflow = false;
};

constexpr double PI = 3.14159265358979323846;

inline double turnsToRadians(uint16_t turns) {
    return 2.0 * PI * turns / 65536.0;
}

inline uint16_t toFixed(double value, double scale) {
    const double scaled = std::round(value * scale);
    return static_cast<uint16_t>(static_cast<int16_t>(std::clamp(scaled, -32767.0, 32767.0)));
}

inline uint16_t power(uint16_t base, uint16_t exponent) {
    uint16_t result = 1;
    while (exponent) {
        if (exponent & 1) result = static_cast<uint16_t>(result * base);
        base = static_cast<uint16_t>(base * base);
        exponent >>= 1;
    }
    return result;
}

inline AluResult runAlu(uint16_t a, uint16_t b, unsigned op) {
    AluResult out;
    uint32_t wide = 0;
    const int16_t signedA = static_cast<int16_t>(a);

    switch (op) {
        case ALU_ADD:
            wide = static_cast<uint32_t>(a) + b;
            out.result = static_cast<uint16_t>(wide);
            break;
        case ALU_SUB:
        case ALU_CMP:
            wide = static_cast<uint32_t>(a) - b;
            out.result = static_cast<uint16_t>(wide);
            break;
        case ALU_MUL:  out.result = static_cast<uint16_t>(static_cast<uint32_t>(a) * b); break;
        case ALU_DIV:  out.result = b ? a / b : 0; break;
        case ALU_MOD:  out.result = b ? a % b : 0; break;
        case ALU_AND:  out.result = a & b; break;
        case ALU_OR:   out.result = a | b; break;
        case ALU_NAND: out.result = static_cast<uint16_t>(~(a & b)); break;
        case ALU_NOR:  out.result = static_cast<uint16_t>(~(a | b)); break;
        case ALU_XOR:  out.result = a ^ b; break;
        case ALU_XNOR: out.result = static_cast<uint16_t>(~(a ^ b)); break;
        case ALU_SHL:  out.result = b < 16 ? static_cast<uint16_t>(a << b) : 0; break;
        case ALU_SHR:  out.result = b < 16 ? static_cast<uint16_t>(a >> b) : 0; break;
        case ALU_MIN:  out.result = std::min(a, b); break;
        case ALU_MAX:  out.result = std::max(a, b); break;
        case ALU_NOT:  out.result = static_cast<uint16_t>(~a); break;
        case ALU_SIN:  out.result = toFixed(std::sin(turnsToRadians(a)), 32767.0); break;
        case ALU_COS:  out.result = toFixed(std::cos(turnsToRadians(a)), 32767.0); break;
        case ALU_TAN:  out.result = toFixed(std::tan(turnsToRadians(a)), 256.0); break;
        case ALU_SQRT: out.result = static_cast<uint16_t>(std::sqrt(static_cast<double>(a))); break;
        case ALU_POW:  out.result = power(a, b); break;
        case ALU_ABS:  out.result = static_cast<uint16_t>(signedA < 0 ? -signedA : signedA); break;
        default: break;
    }

    out.zero = out.result == 0;
    out.negative = out.result & 0x8000;

    const bool signA = a & 0x8000;
    const bool signB = b & 0x8000;
    const bool signR = out.result & 0x8000;

    if (op == ALU_ADD) {
        out.carry = wide > 0xFFFF;
        out.overflow = (signA == signB) && (signA != signR);
    }
    else if (op == ALU_SUB || op == ALU_CMP) {
        out.carry = a < b;
        out.overflow = (signA != signB) && (signA != signR);
    }

    return out;
}

}

Core::Core() : ram(ADDRESS_SPACE, 0) {
    std::random_device device;
    rngState = device() | 1u;
}

uint16_t Core::nextRandom() {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return static_cast<uint16_t>(rngState >> 8);
}

void Core::writeRam(uint16_t address, uint16_t data) {
    ram[address] = data;
    ++ramWrites;
    if (address < ramDirtyLo) ramDirtyLo = address;
    if (address > ramDirtyHi) ramDirtyHi = address;
    ramDirty = true;

    const uint16_t offset = address - SCREEN_BASE;
    if (offset < SCREEN_PIXELS) {
        screen[offset] = data;
        screenDirty = true;
    }
}

void Core::halfTick() {
    const Decoded& cu = decodeTable[ir0 >> 9];
    const unsigned subOp = (ir0 >> 9) & 15;
    const unsigned srcRA = (ir0 >> 3) & 7;
    const unsigned srcRB = ir0 & 7;
    const unsigned dstR = (ir0 >> 6) & 7;

    const bool pExt = ffExt;
    const bool pAddr = ffAddrBase || ffAddrJsr;
    const bool pSp = ffSpChange;
    const bool pJsr = ffJsr;
    const bool pRegAddr = ffRegAddr;

    const uint16_t prePc = pc;
    const uint16_t preSp = sp;
    const uint16_t preRamAddr = ramAddr;
    const uint16_t oldRA = r[srcRA];
    const uint16_t oldRB = r[srcRB];

    const bool aluEnabled = cu.aluEnabled;
    const AluResult alu = aluEnabled ? runAlu(oldRA, oldRB, cu.aluOp) : AluResult{};

    const bool containsAddress = cu.addressMode == ADDR_ALWAYS ||
                                 (cu.addressMode == ADDR_IF_JUMP_TAKEN && jumpTaken(subOp, flags));

    clockValue = !clockValue;
    ++tickCount;
    const bool clk = clockValue && !cu.hlt;

    const bool regWrite = cu.regWrite && (pExt || aluEnabled || pRegAddr || cu.useIn);

    const bool inJsr = cu.jsr && !pJsr;
    const bool inContainsAddress = inJsr || (containsAddress && pExt);
    const bool inSpChange = cu.spChange && !pSp && !pExt;

    if (clk) {
        ffExt = pJsr || (cu.nextExt && !pExt);
        ffAddrJsr = inContainsAddress;
        ffSpChange = inSpChange;
        ffJsr = inJsr && !pExt;
        ffRts = cu.rts;
        ffRegAddr = cu.regIsAddress && !pRegAddr;
    }
    else {
        ffAddrBase = inContainsAddress && !inJsr;
    }

    if (regWrite && (pAddr ? clk : !clk)) {
        r[dstR] = cu.useIn ? (cu.ioPort == RANDOM_PORT ? nextRandom() : io[cu.ioPort])
                : (pSp || pRegAddr) ? ramOut
                : pExt ? ir1
                : alu.result;
    }

    if (clk && !ffJsr && !ffSpChange && !inSpChange && !pRegAddr) {
        const bool writeToPc = (cu.loadPC && pExt && !pJsr && pAddr) || cu.rts;
        pc = writeToPc ? (cu.rts ? ramOut : ir1) : static_cast<uint16_t>(pc + 1);
    }

    if (pSp && !clk)
        sp = cu.spPop ? sp + 1 : sp - 1;

    if (clk && cu.flagsWrite)
        flags = alu.zero | (alu.negative << 1) | (alu.carry << 2) | (alu.overflow << 3);

    if (!clk)
        ramAddr = ir1;

    const bool memOrJsr = (pSp && !cu.spPop) || (cu.memWrite && (pExt || pRegAddr)) || pJsr;
    const bool pastWrite = writeFF;
    if (!clk)
        writeFF = !pExt && !writeFF && memOrJsr;

    if ((memOrJsr && !pastWrite) || (pRegAddr && !regWrite)) {
        if (pSp ? !clk : clk) {
            uint16_t address = prePc;
            if ((!pJsr && pAddr && pExt && !cu.loadPC) || pSp || pRegAddr)
                address = pSp ? preSp : pRegAddr ? oldRB : preRamAddr;
            writeRam(address, pJsr ? static_cast<uint16_t>(prePc + 2) : oldRA);
        }
    }

    const bool nExt = ffExt;
    const bool nSp = ffSpChange;
    const bool nJsr = ffJsr;
    const bool nRegAddr = ffRegAddr;

    uint16_t fetchAddress = pc;
    if ((!nJsr && (ffAddrBase || ffAddrJsr) && nExt && !cu.loadPC) || nSp || nRegAddr)
        fetchAddress = nSp ? sp : nRegAddr ? r[srcRB] : ramAddr;

    if (!clk)
        writeFF = !nExt && !writeFF && nJsr;

    const uint16_t newRam = ram[fetchAddress];

    if (cu.useOut && cu.ioPort != RANDOM_PORT) {
        const uint16_t value = r[srcRA];
        if (io[cu.ioPort] != value) {
            io[cu.ioPort] = value;
            ioDirty = true;
        }
    }

    if (!clk && !nExt && !nRegAddr && !nSp)
        ir0 = newRam;
    if (nExt)
        ir1 = newRam;

    ramOut = newRam;
    lastAddress = fetchAddress;
    touched[fetchAddress >> 6] |= 1ull << (fetchAddress & 63);
}

Core::StateKey Core::stateKey() const {
    StateKey key{};
    std::copy(r.begin(), r.end(), key.begin());
    key[8] = sp;
    key[9] = pc;
    key[10] = ramAddr;
    key[11] = ir0;
    key[12] = ir1;
    key[13] = ramOut;
    key[14] = flags | (ffExt << 4) | (ffAddrBase << 5) | (ffAddrJsr << 6) | (ffSpChange << 7) |
              (ffJsr << 8) | (ffRts << 9) | (ffRegAddr << 10) | (writeFF << 11);
    return key;
}

void Core::run(uint64_t halfTicks) {
    while (halfTicks) {
        if (!decodeTable[ir0 >> 9].hlt) {
            halfTick();
            --halfTicks;
            continue;
        }

        const StateKey before = stateKey();
        halfTick();
        --halfTicks;
        if (stateKey() == before) {
            clockValue ^= halfTicks & 1;
            tickCount += halfTicks;
            return;
        }
    }
}

void Core::init() {
    ir0 = ram[0];
    ramOut = ram[0];
    sp = 0xFFFF;
    lastAddress = 0;
    io.fill(0);
    ioDirty = true;
    touched.fill(0);
}

void Core::reset() {
    r.fill(0);
    sp = pc = ramAddr = ir0 = ir1 = ramOut = 0;
    flags = 0;
    clockValue = false;
    ffExt = ffAddrBase = ffAddrJsr = ffSpChange = ffJsr = ffRts = ffRegAddr = false;
    writeFF = false;
    tickCount = 0;
    screen.fill(0);
    screenDirty = true;
    init();
}

uint16_t Core::getReg(RegisterName name) const {
    switch (name) {
        case SP: return sp;
        case PC: return pc;
        case FLAGS: return flags;
        case IR0: return ir0;
        case IR1: return ir1;
        case RAM_ADDRESS: return ramAddr;
        default: return r[name & 7];
    }
}

void Core::setReg(RegisterName name, uint16_t value) {
    switch (name) {
        case SP: sp = value; break;
        case PC: pc = value; break;
        case FLAGS: flags = value & 15; break;
        case IR0: ir0 = value; break;
        case IR1: ir1 = value; break;
        case RAM_ADDRESS: ramAddr = value; break;
        default: r[name & 7] = value; break;
    }
}

bool Core::loadRam(const std::vector<uint16_t>& words) {
    if (words.size() != ADDRESS_SPACE)
        return false;
    ram = words;
    ramDirtyLo = 0;
    ramDirtyHi = 0xFFFF;
    ramDirty = true;
    return true;
}

void Core::poke(uint16_t address, uint16_t value) {
    ram[address] = value;
    if (address < ramDirtyLo) ramDirtyLo = address;
    if (address > ramDirtyHi) ramDirtyHi = address;
    ramDirty = true;
}
