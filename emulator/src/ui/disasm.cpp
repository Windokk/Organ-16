#include "disasm.hpp"

#include <array>

namespace {

struct Entry {
    const char* name = nullptr;
    char fmt = 'W';
    int words = 1;
};

std::array<Entry, 128> buildTable() {
    std::array<Entry, 128> t{};
    auto def = [&](int op, int sub, const char* name, char fmt, int words = 1) { t[op * 16 + sub] = {name, fmt, words}; };

    const char* alu[16] = {"NOP", "SUB", "MUL", "DIV", "MOD", "AND", "OR", "NAND", "NOR", "XOR", "ADD", "SHL", "SHR", "XNOR", "MIN", "MAX"};
    for (int i = 0; i < 16; ++i)
        def(0, i, alu[i], i == 0 ? '-' : 'T');

    def(1, 0, "SIN", 'P'); def(1, 1, "COS", 'P'); def(1, 2, "TAN", 'P'); def(1, 3, "SQRT", 'P');
    def(1, 4, "POW", 'T'); def(1, 5, "ABS", 'P'); def(1, 6, "INC", 'd'); def(1, 7, "DEC", 'd');
    def(1, 8, "COMPI", 'i', 2); def(1, 9, "COMPM", 'm', 2); def(1, 10, "CMP", 'c'); def(1, 11, "NOT", 'P');
    def(2, 0, "MOVI", 'I', 2); def(2, 1, "MOV", 'P');
    def(3, 0, "LOAD", 'L', 2); def(3, 1, "STORE", 'S', 2); def(3, 2, "STORER", 'c');
    def(3, 3, "LOADR", 'R'); def(3, 4, "LOADRP", 'R'); def(3, 5, "STORERP", 'c');

    const char* jumps[11] = {"JMP", "JE", "JNE", "JB", "JBE", "JA", "JAE", "JL", "JLE", "JG", "JGE"};
    for (int i = 0; i < 11; ++i)
        def(4, i, jumps[i], 'J', 2);
    def(4, 11, "JSR", 'J', 2); def(4, 12, "RTS", '-');

    def(5, 0, "PUSH", 'a'); def(5, 1, "POP", 'D');
    def(7, 0, "HLT", '-'); def(7, 1, "IN", 'N', 2); def(7, 2, "OUT", 'O', 2); def(7, 3, "RAND", 'D');
    return t;
}

const std::array<Entry, 128>& opTable() {
    static const auto t = buildTable();
    return t;
}

QString regName(int n) { return "R" + QString::number(n); }

}

QString hex(unsigned value, int digits) {
    return "0x" + QString::number(value, 16).rightJustified(digits, '0').toUpper();
}

Insn disassemble(const uint16_t* ram, uint16_t address) {
    Insn insn;
    insn.w = ram[address];
    const int op = insn.w >> 13;
    const int sub = (insn.w >> 9) & 15;
    const Entry& e = opTable()[op * 16 + sub];
    insn.d = (insn.w >> 6) & 7;
    insn.a = (insn.w >> 3) & 7;
    insn.b = insn.w & 7;
    if (!e.name) {
        insn.name = ".word";
        insn.fmt = 'W';
        return insn;
    }
    insn.name = e.name;
    insn.fmt = e.fmt;
    insn.words = e.words;
    if (e.words == 2)
        insn.imm = ram[static_cast<uint16_t>(address + 1)];
    return insn;
}

std::vector<OperandPart> operands(const Insn& i) {
    const QString ra = regName(i.a), rb = regName(i.b), rd = regName(i.d);
    switch (i.fmt) {
        case 'W': return {{hex(i.w), false}};
        case 'T': return {{rd + ", " + ra + ", " + rb, false}};
        case 'P': return {{rd + ", " + ra, false}};
        case 'c': return {{ra + ", " + rb, false}};
        case 'd': return {{i.b != i.d ? rd + ", " + rb : rd, false}};
        case 'D': return {{rd, false}};
        case 'a': return {{ra, false}};
        case 'I': return {{rd + ", " + hex(i.imm), false}};
        case 'i': return {{ra + ", " + hex(i.imm), false}};
        case 'L': return {{rd + ", ", false}, {hex(i.imm), true}};
        case 'N': return {{rd + ", " + hex(i.imm), false}};
        case 'O': return {{ra + ", " + hex(i.imm), false}};
        case 'S':
        case 'm': return {{ra + ", ", false}, {hex(i.imm), true}};
        case 'R': return {{rd + ", " + rb, false}};
        case 'J': return {{hex(i.imm), true}};
        default: return {};
    }
}

QString formatInsn(const Insn& insn) {
    QString out = insn.name;
    QString joined;
    for (const OperandPart& part : operands(insn))
        joined += part.text;
    if (!joined.isEmpty())
        out += " " + joined;
    return out;
}
