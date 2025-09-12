#include "clock.hpp"

Clock* Clock::instancePtr = nullptr;
std::mutex Clock::mtx;