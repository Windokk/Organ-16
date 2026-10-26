#pragma once

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

        uint64_t halfTicksRun() const { return executed.load(); }

    private:
        void loop();

        Core& core;
        std::thread worker;
        std::atomic<bool> running{false};
        std::atomic<bool> automatic{false};
        std::atomic<double> frequency{0.0};
        std::atomic<uint64_t> executed{0};
};
