#pragma once

#include <array>
#include <stdexcept>
#include <string>

enum RegisterName {
    R0, R1, R2, R3, R4, R5, R6, R7,
    SP, PC, FLAGS, IR0, IR1, RAM_ADDRESS
};

inline RegisterName RegisterFromString(const std::string& reg) {
    static const std::array<const char*, 14> names = {
        "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
        "SP", "PC", "FLAGS", "IR0", "IR1", "RAM_ADDRESS"
    };
    for (size_t i = 0; i < names.size(); ++i) {
        if (reg == names[i])
            return static_cast<RegisterName>(i);
    }
    throw std::invalid_argument("Invalid register name: " + reg);
}
