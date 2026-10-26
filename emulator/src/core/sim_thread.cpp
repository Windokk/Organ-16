#include "sim_thread.hpp"

#include <algorithm>
#include <chrono>

namespace {

constexpr uint64_t RUN_CHUNK = 8192;
constexpr double MAX_CATCH_UP_SECONDS = 0.05;

}

SimThread::SimThread(Core& core) : core(core) {}

SimThread::~SimThread() {
    stop();
}

void SimThread::start() {
    if (running.exchange(true))
        return;
    worker = std::thread(&SimThread::loop, this);
}

void SimThread::stop() {
    running.store(false);
    if (worker.joinable())
        worker.join();
}

void SimThread::loop() {
    using Clock = std::chrono::steady_clock;

    double halfTickDebt = 0;
    bool wasAutomatic = false;
    Clock::time_point last = Clock::now();

    while (running.load()) {
        const double hz = frequency.load();
        if (!automatic.load() || hz <= 0) {
            wasAutomatic = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        const Clock::time_point now = Clock::now();
        if (!wasAutomatic) {
            wasAutomatic = true;
            halfTickDebt = 0;
            last = now;
        }

        const double elapsed = std::min(std::chrono::duration<double>(now - last).count(), MAX_CATCH_UP_SECONDS);
        last = now;
        halfTickDebt = std::min(halfTickDebt + 2.0 * hz * elapsed, 2.0 * hz * MAX_CATCH_UP_SECONDS);

        if (halfTickDebt < 1.0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        while (halfTickDebt >= 1.0 && running.load() && automatic.load()) {
            const uint64_t chunk = std::min<uint64_t>(static_cast<uint64_t>(halfTickDebt), RUN_CHUNK);
            {
                std::unique_lock<std::mutex> guard = core.lock();
                core.run(chunk);
            }
            executed.fetch_add(chunk);
            halfTickDebt -= static_cast<double>(chunk);

            while (core.hasWaiters())
                std::this_thread::yield();
        }
    }
}
