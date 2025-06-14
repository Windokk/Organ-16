import re
from typing import Tuple, List, Optional

def ReadFileLines(filename: str):
    try:
        with open(filename, 'r') as file:
            lines = []
            for line in file:
                # Strip comments starting with ; or #
                line = line.split(';')[0].split('#')[0].strip()
                if line:
                    lines.append(line)
            return lines
    except FileNotFoundError:
        print(f"File '{filename}' not found.")
        return []

def ImmediateToBin(immediate: str, label_map: dict = None) -> str:
    try:
        if immediate.lower().startswith("0x"):
            value = int(immediate, 16)
        elif immediate.lower().startswith("0b"):
            value = int(immediate, 2)
        elif immediate.isdigit():
            value = int(immediate, 10)
        elif label_map and immediate.upper() in label_map:
            value = label_map[immediate.upper()]
        else:
            raise ValueError(f"Invalid immediate or unknown label: {immediate}")
        return bin(value)[2:].zfill(16)
    except ValueError:
        raise ValueError(f"Invalid immediate value: {immediate}")

def OpCodeToBinOpCode(instruction: str) -> str:
    instruction = instruction.upper().strip()
    opcode_map = {
        "ADD":  ("000", "0000"), "SUB":  ("000", "0001"), "MUL":  ("000", "0010"),
        "DIV":  ("000", "0011"), "MOD":  ("000", "0100"), "AND":  ("000", "0101"),
        "OR":   ("000", "0110"), "NAND": ("000", "0111"), "NOR":  ("000", "1000"),
        "XOR":  ("000", "1001"), "NOT":  ("001", "1011"), "CMP":  ("001", "1010"),
        "MOV":  ("010", "0000"), "LOAD": ("011", "0000"), "STORE":("011", "0001"),
        "JMP":  ("100", "0000"), "JE":   ("100", "0001"), "JNE":  ("100", "0010"),
        "JB":   ("100", "0011"), "JBE":  ("100", "0100"), "JA":   ("100", "0101"),
        "JAE":  ("100", "0110"), "JL":   ("100", "0111"), "JLE":  ("100", "1000"),
        "JG":   ("100", "1001"), "JGE":  ("100", "1010"), "JSR":  ("100", "1011"),
        "RTS":  ("100", "1100"), "HLT":  ("111", "0000"), "PUSH": ("101", "0000"),
        "POP":  ("101", "0001"),
    }
    if instruction in opcode_map:
        op, sub = opcode_map[instruction]
        return op + sub
    else:
        raise ValueError(f"Unknown instruction: {instruction}")

def DataToBinData(data: list) -> str:
    register_map = {
        "R0": "000", "R1": "001", "R2": "010", "R3": "011",
        "R4": "100", "R5": "101", "R6": "110", "R7": "111"
    }
    return "".join(register_map[reg] for reg in data)

def ParseLine(line: str) -> Tuple[str, List[str], Optional[str]]:
    line = line.split(';')[0].strip()
    if not line:
        return "", [], None
    parts = line.split(maxsplit=1)
    instruction = parts[0].upper()
    operands = parts[1] if len(parts) > 1 else ""
    tokens = [tok.upper() for tok in operands.replace(",", " ").split()]
    regs = [tok for tok in tokens if re.fullmatch(r"R[0-7]", tok)]
    imm = next((tok for tok in tokens if not re.fullmatch(r"R[0-7]", tok)), None)
    return instruction, regs, imm

def IsLabel(line: str):
    return line.endswith(":") and re.match(r"^[a-zA-Z_][a-zA-Z0-9_]*:$", line)

def InstructionWordCount(line: str) -> int:
    instr, regs, imm = ParseLine(line)
    if not instr:
        return 0
    return 2 if imm else 1

def FirstPass(lines: List[str], start_address: int) -> dict:
    labels = {}
    address = start_address
    for line in lines:
        if IsLabel(line):
            label = line[:-1].upper()
            labels[label] = address
        else:
            address += InstructionWordCount(line)
    return labels

def ReplaceLabels(line: str, label_map: dict) -> str:
    instr, regs, imm = ParseLine(line)
    if imm and imm.upper() in label_map:
        line = line.replace(imm, f"0x{label_map[imm.upper()]:04X}")
    return line

def LineToBinary(line: str, label_map: dict = None):
    instr, regs, imm = ParseLine(line)
    if not instr:
        return None
    op_bin = OpCodeToBinOpCode(instr)
    if len(regs) == 0:
        if imm:
            return op_bin + "000000000", ImmediateToBin(imm, label_map)
        return op_bin + "000000000"
    elif len(regs) == 1:
        if imm:
            if op_bin == "0110001":
                return op_bin + "000" + DataToBinData(regs) + "000", ImmediateToBin(imm, label_map)
            else:
                return op_bin + DataToBinData(regs) + "000000", ImmediateToBin(imm, label_map)
        if op_bin == "1010001":
            return op_bin + DataToBinData(regs) + "000000"
        else:
            return op_bin + "000" + DataToBinData(regs) + "000"
    elif len(regs) == 2:
        if(op_bin == "0011010"):
            return op_bin + "000" + DataToBinData(regs)
        else:
            return op_bin + DataToBinData(regs) + "000"
    elif len(regs) == 3:
        return op_bin + DataToBinData(regs)

def BinToHex(binary: str) -> str:
    return hex(int(binary, 2))[2:].zfill(4).lower()

def CompileMultiple(segments: List[Tuple[str, int]], output_file: str):
    memory = ["0000"] * 65536  # 64K words initialized to zero

    global_labels = {}
    file_lines = []

    for filepath, base_addr in segments:
        lines = ReadFileLines(filepath)
        label_map = FirstPass(lines, base_addr)
        global_labels.update(label_map)
        file_lines.append((lines, base_addr))

    for lines, base_addr in file_lines:
        addr = base_addr
        for line in lines:
            if IsLabel(line):
                continue
            line = ReplaceLabels(line, global_labels)
            binaries = LineToBinary(line, global_labels)
            if isinstance(binaries, (tuple, list)):
                for b in binaries:
                    memory[addr] = BinToHex(b)
                    addr += 1
            elif binaries:
                memory[addr] = BinToHex(binaries)
                addr += 1

    with open(output_file, "w") as out:
        for i in range(0, len(memory), 16):
            out.write(" ".join(memory[i:i+16]) + "\n")

if __name__ == "__main__":
    segments = [
        ("first.pglu", 0x0000),
        ("second.pglu", 0x3000),
    ]
    CompileMultiple(segments, "out.bin")