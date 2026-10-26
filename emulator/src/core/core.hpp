#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include "decode.hpp"
#include "registers.hpp"

constexpr size_t ADDRESS_SPACE = 65536;
constexpr uint16_t SCREEN_BASE = 0x8000;
constexpr size_t SCREEN_WIDTH = 128;
constexpr size_t SCREEN_HEIGHT = 128;
constexpr size_t SCREEN_PIXELS = SCREEN_WIDTH * SCREEN_HEIGHT;

class Core {
    public:
        Core();

        void init();
        void reset();
        void run(uint64_t halfTicks);
        void halfTick();

        uint16_t getReg(RegisterName name) const;
        void setReg(RegisterName name, uint16_t value);

        bool loadRam(const std::vector<uint16_t>& words);
        void poke(uint16_t address, uint16_t value);

        void clearRamDirty() { ramDirtyLo = 0xFFFF; ramDirtyHi = 0; ramDirty = false; }

        std::unique_lock<std::mutex> lock() const {
            waiters.fetch_add(1, std::memory_order_relaxed);
            std::unique_lock<std::mutex> guard(mutex);
            waiters.fetch_sub(1, std::memory_order_relaxed);
            return guard;
        }
        bool hasWaiters() const { return waiters.load(std::memory_order_relaxed) > 0; }

        std::vector<uint16_t> ram;
        std::array<uint16_t, SCREEN_PIXELS> screen{};
        std::array<uint64_t, ADDRESS_SPACE / 64> touched{};

        std::array<uint16_t, 8> r{};
        uint16_t sp = 0xFFFF;
        uint16_t pc = 0;
        uint16_t ramAddr = 0;
        uint16_t ir0 = 0;
        uint16_t ir1 = 0;
        uint16_t ramOut = 0;
        uint8_t flags = 0;

        bool clockValue = false;
        bool ffExt = false;
        bool ffAddrBase = false;
        bool ffAddrJsr = false;
        bool ffSpChange = false;
        bool ffJsr = false;
        bool ffRts = false;
        bool ffRegAddr = false;
        bool writeFF = false;

        std::array<uint16_t, 3> io{};
        bool ioDirty = false;
        bool screenDirty = false;
        bool ramDirty = false;
        uint16_t ramDirtyLo = 0xFFFF;
        uint16_t ramDirtyHi = 0;
        uint16_t lastAddress = 0;
        uint64_t tickCount = 0;
        uint64_t ramWrites = 0;

    private:
        mutable std::mutex mutex;
        mutable std::atomic<int> waiters{0};

        using StateKey = std::array<uint16_t, 16>;
        StateKey stateKey() const;
        void writeRam(uint16_t address, uint16_t data);
        uint16_t nextRandom();
        uint32_t rngState = 1;
};
