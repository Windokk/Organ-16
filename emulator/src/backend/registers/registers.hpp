#pragma once

#include <array>
#include <string>
#include <stdexcept>
#include <mutex>
#include <unordered_map>
#include <iostream>

enum RegisterName{
    R0 = 0b000,
    R1 = 0b001,
    R2 = 0b010,
    R3 = 0b011,
    R4 = 0b100,
    R5 = 0b101,
    R6 = 0b110,
    R7 = 0b111,
    SP,
    PC,
    FLAGS,
    IR0,
    IR1,
    RAM_ADDRESS
};

inline RegisterName RegisterFromString(const std::string& reg) {
    static const std::array<std::string, 14> registerNames = {
        "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
        "SP", "PC", "FLAGS", "IR0", "IR1", "RAM_ADDRESS"
    };

    for (size_t i = 0; i < registerNames.size(); ++i) {
        if (registerNames[i] == reg)
            return static_cast<RegisterName>(i);
    }

    throw std::invalid_argument("Invalid register name: " + reg);
}

void UpdateRegValue(RegisterName name, uint16_t value);

template<typename T, T MASK = static_cast<T>(-1)>
class Register {
        static_assert(std::is_unsigned<T>::value, "Register type must be unsigned");

    private:
        T value;
        RegisterName name;

    public:
        Register(RegisterName name) : value(0) {
            this->name = name;
        }

        // Happens when 
        T updateValue(T data, bool reset){
            if (reset) {
                value = 0;
            }
            else {
                value = data & MASK;
            }
            UpdateRegValue(name, value);
            return value;
        }

        T get() const {
            return value;
        }
};


using Register4 = Register<uint8_t, 0x0F>;     // 4-bit register
using Register16 = Register<uint16_t>;        // 16-bit register

struct RegsOutOnChange{
    uint16_t RA = 0;
    uint16_t RB = 0;
    uint16_t PC = 0;
    uint16_t SP = 0;
    uint8_t FLAGS = 0;
    uint16_t RAM_ADDRESS = 0;
};

struct RegsOutOnIdle{
    uint16_t IR0 = 0;
    uint16_t IR1 = 0;
};

struct RegsOut{
    uint16_t R0 = 0;
    uint16_t R1 = 0;
    uint16_t R2 = 0;
    uint16_t R3 = 0;
    uint16_t R4 = 0;
    uint16_t R5 = 0;
    uint16_t R6 = 0;
    uint16_t R7 = 0;
    uint16_t PC = 0;
    uint16_t SP = 0;
    uint8_t FLAGS = 0;
    uint16_t RAM_ADDRESS = 0;
    uint16_t IR0 = 0;
    uint16_t IR1 = 0;
};

struct RegsInOnChange{
    //gp regs
    uint16_t gpData = 0;
    bool gpClock = false;
    bool gpRegWrite = false;
    uint8_t gpRegToWrite = 0;
    uint8_t raRead = 0;
    uint8_t rbRead = 0;

    //program counter
    bool pcClock = false;
    uint16_t pcData;
    bool writeToPC;

    //stack pointer
    bool spPop = false;
    bool spClock = false;

    //flags reg
    bool zero = false;
    bool negative = false;
    bool carry = false;
    bool overflow = false;
    bool flagsWrite = false;
    bool flagsClock = false;

    //ram address reg
    bool ramAddrClock = false;
};

struct RegsInOnIdle{
    
    //instructions regs
    bool ir0Clock;
    bool ir1Clock;
    bool ir0Write;
    uint16_t ir0Data;
    uint16_t ir1Data;
};

class RegisterFile{
    private:
        RegisterFile() {}

        // Static pointer to the RegisterFile instance
        static RegisterFile* instancePtr;

        // Mutex to ensure thread safety
        static std::mutex mtx;

        std::unordered_map<RegisterName, Register16> gpRegs = {
            {R0, Register16(R0)},
            {R1, Register16(R1)},
            {R2, Register16(R2)},
            {R3, Register16(R3)},
            {R4, Register16(R4)},
            {R5, Register16(R5)},
            {R6, Register16(R6)},
            {R7, Register16(R7)}
        };
        std::unordered_map<RegisterName, Register16> specRegs= {
            {SP, Register16(SP)},
            {PC, Register16(PC)},
            {RAM_ADDRESS, Register16(RAM_ADDRESS)}
        };
        std::unordered_map<RegisterName, Register16> instructionsRegs = {
            {IR0, Register16(IR0)},
            {IR1, Register16(IR1)}
        };

        Register4 flagReg = Register4(FLAGS);

    public:

        RegisterFile(const RegisterFile&) = delete;
        RegisterFile& operator=(const RegisterFile&) = delete;
        RegisterFile(RegisterFile&&) = delete;
        RegisterFile& operator=(RegisterFile&&) = delete;

        // Static method to get the Singleton instance
        static RegisterFile* GetInstance() {
            if (instancePtr == nullptr) {
                std::lock_guard<std::mutex> lock(mtx);
                if (instancePtr == nullptr) {
                    instancePtr = new RegisterFile();
                }
            }
            return instancePtr;
        }

        RegsOutOnChange OnClockChange(RegsInOnChange in);

        RegsOutOnIdle OnClockIdle(RegsInOnIdle in);

        void SetRegValue(RegisterName name, uint16_t value){
            switch(name){
                case R0:
                case R1:
                case R2:
                case R3:
                case R4:
                case R5:
                case R6:
                case R7:
                    gpRegs.at(name).updateValue(value, false);
                    break;
                case SP:
                case PC:
                case RAM_ADDRESS:
                    specRegs.at(name).updateValue(value, false);
                    break;
                case FLAGS:
                    flagReg.updateValue(value & 15, false);
                    break;
                case IR0:
                case IR1:
                    instructionsRegs.at(name).updateValue(value, false);
                    break;
                default:
                    break;
            }
        }

        uint16_t GetRegValue(RegisterName name){
            switch(name){
                case R0:
                case R1:
                case R2:
                case R3:
                case R4:
                case R5:
                case R6:
                case R7:
                    return gpRegs.at(name).get();
                case SP:
                case PC:
                case RAM_ADDRESS:
                    return specRegs.at(name).get();
                case FLAGS:
                    return flagReg.get();
                case IR0:
                case IR1:
                    return instructionsRegs.at(name).get();
                default:
                    return 0;
            }
            return 0;
        }
    
        RegsOut GetRegsValues(){
            RegsOut out;
            out.R0 = gpRegs.at(R0).get();
            out.R1 = gpRegs.at(R1).get();
            out.R2 = gpRegs.at(R2).get();
            out.R3 = gpRegs.at(R3).get();
            out.R4 = gpRegs.at(R4).get();
            out.R5 = gpRegs.at(R5).get();
            out.R6 = gpRegs.at(R6).get();
            out.R7 = gpRegs.at(R7).get();
            out.SP = specRegs.at(SP).get();
            out.PC = specRegs.at(PC).get();
            out.RAM_ADDRESS = specRegs.at(RAM_ADDRESS).get();
            out.FLAGS = flagReg.get();
            out.IR0 = instructionsRegs.at(IR0).get();
            out.IR1 = instructionsRegs.at(IR1).get();
            return out;
        }

        void Reset(){
            for (auto& pair : gpRegs) {
                pair.second.updateValue(0, true);
            }
            for (auto& pair : specRegs) {
                pair.second.updateValue(0, true);
            }
            for (auto& pair : instructionsRegs) {
                pair.second.updateValue(0, true);
            }
        };
    };
