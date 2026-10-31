#pragma once

#include <QString>
#include <cstdint>
#include <vector>

struct Insn {
    QString name;
    char fmt = 'W';
    int words = 1;
    uint16_t w = 0;
    uint16_t imm = 0;
    int d = 0, a = 0, b = 0;
};

struct OperandPart {
    QString text;
    bool address = false;
};

Insn Disassemble(const uint16_t* ram, uint16_t address);
std::vector<OperandPart> Operands(const Insn& insn);
QString FormatInsn(const Insn& insn);
QString Hex(unsigned value, int digits = 4);
