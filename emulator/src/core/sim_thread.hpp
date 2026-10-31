#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <thread>

#include "core.hpp"

class SimThread {
    public:
        explicit SimThread(Core& core);
        ~SimThread();

        SimThread(const SimThread&) = delete;
        SimThread& operator=(const SimThread&) = delete;

        void start();
        void stop();

        void setAutomatic(bool enabled) { automatic.store(enabled); }
        void setFrequency(double hz) { frequency.store(hz); }

        bool automaticEnabled() const { return automatic.load(); }
        uint64_t halfTicksRun() const { return executed.load(); }

        void setBreakpoint(uint16_t address, bool enabled);
        bool breakHit() const { return hit.load(); }
        void clearBreakHit() { hit.store(false); }

    private:
        void loop();

        Core& core;
        std::thread worker;
        std::atomic<bool> running{false};
        std::atomic<bool> automatic{false};
        std::atomic<double> frequency{0.0};
        std::atomic<uint64_t> executed{0};
        std::array<std::atomic<uint8_t>, ADDRESS_SPACE> breakpoints{};
        std::atomic<int> breakpointCount{0};
        std::atomic<bool> hit{false};
};
