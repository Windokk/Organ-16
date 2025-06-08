import re
from typing import Tuple, List, Optional

def ReadFileLines(filename:str):
    try:
        with open(filename, 'r') as file:
            lines = file.readlines()
        return lines
    except FileNotFoundError:
        print(f"File '{filename}' not found.")
        return []

def ImmediateToBin(immediate: str) -> str:
    try:
        if immediate.lower().startswith("0x"):
            value = int(immediate, 16)
        elif immediate.lower().startswith("0b"):
            value = int(immediate, 2)
        else:
            value = int(immediate, 10)

        return bin(value)[2:].zfill(16)
    except ValueError:
        raise ValueError(f"Invalid immediate value: {immediate}")

def OpCodeToBinOpCode(instruction: str) -> str:
    instruction = instruction.upper().strip()

    opcode_map = {
        "ADD":  ("000", "0000"),
        "SUB":  ("000", "0001"),
        "MUL":  ("000", "0010"),
        "DIV":  ("000", "0011"),
        "MOD":  ("000", "0100"),
        "AND":  ("000", "0101"),
        "OR":   ("000", "0110"),
        "NAND": ("000", "0111"),
        "NOR":  ("000", "1000"),
        "XOR":  ("000", "1001"),
        "NOT":  ("001", "1011"),
        "CMP":  ("001", "1010"),
        "MOV":  ("010", "0000"),
        "LOAD": ("011", "0000"),
        "STORE":("011", "0001"),
        "JMP":  ("100", "0000"),
        "JE":   ("100", "0001"),
        "JNE":  ("100", "0010"),
        "JB":   ("100", "0011"),
        "JBE":  ("100", "0100"),
        "JA":   ("100", "0101"),
        "JAE":  ("100", "0110"),
        "JL":   ("100", "0111"),
        "JLE":  ("100", "1000"),
        "JG":   ("100", "1001"),
        "JGE":  ("100", "1010"),
        "JSR":  ("100", "1011"),
        "RTS":  ("100", "1100"),
        "HLT":  ("111", "0000"),
        "PUSH": ("101", "0000"),
        "POP":  ("101", "0001"),
    }

    if instruction in opcode_map:
        op, sub = opcode_map[instruction]
        return op + sub
    else:
        raise ValueError(f"Unknown instruction: {instruction}")

def DataToBinData(data: list) -> str:

    register_map = {
        "R0": "000",
        "R1": "001",
        "R2": "010",
        "R3": "011",
        "R4": "100",
        "R5": "101",
        "R6": "110",
        "R7": "111"
    }

    result = ""
    for reg in data:
        result += register_map[reg]

    return result

def ParseLine(line: str) -> Tuple[str, List[str], Optional[str]]:
    line = line.strip()
    if not line:
        return "", [], ""  # Empty line case

    parts = line.split(maxsplit=1)
    instruction = parts[0].upper()
    operands = parts[1] if len(parts) > 1 else ""

    operands = operands.replace(",", " ")

    tokens = [token.strip().upper() for token in operands.split() if token.strip()]

    registers = []
    immediate = None

    for token in tokens:
        if re.fullmatch(r"R[1-7]", token, re.IGNORECASE):
            if len(registers) < 3:
                registers.append(token.upper())
        else:
            immediate = token.lower()
            break

    return instruction, registers, immediate

def LineToBinary(line: str):
    if not line:
        return
    parsed = ParseLine(line)
    if len(parsed[1]) == 0:
        # Format is JMP or HLT
        if parsed[2]:
            # Format is JMP
            return OpCodeToBinOpCode(parsed[0]) + "000000000", ImmediateToBin(parsed[2])
        else:
            # Format is HLT
            return OpCodeToBinOpCode(parsed[0]) + "000000000"
    elif len(parsed[1]) == 1:
        # Format is either R, RI, or MEM
        if parsed[2]:
            # Format is either RI or MEM
            if OpCodeToBinOpCode(parsed[0]) == "0110001":
                return OpCodeToBinOpCode(parsed[0]) + "000" + DataToBinData(parsed[1]) + "000", ImmediateToBin(parsed[2])
            else:
                return OpCodeToBinOpCode(parsed[0])+DataToBinData(parsed[1])+"000000", ImmediateToBin(parsed[2])
        else:
            # Format is R
            return OpCodeToBinOpCode(parsed[0])+DataToBinData(parsed[1])+"000000"
    elif len(parsed[1]) == 2:
        # Format is RR
        return OpCodeToBinOpCode(parsed[0]) + DataToBinData(parsed[1])+"000"
    elif len(parsed[1]) == 3:
        # Format is RRR
        return OpCodeToBinOpCode(parsed[0]) + DataToBinData(parsed[1])

def BinToHex(binary: str) -> str:
    binary = binary.strip().replace(" ", "")

    if len(binary) != 16 or not all(c in "01" for c in binary):
        raise ValueError("Input must be a 16-bit binary string (only 0 and 1).")

    hex_value = hex(int(binary, 2))[2:].upper().zfill(4)
    return hex_value.lower()

def Compile(input_file: str, output_file: str):
    with open(input_file, "r") as infile, open(output_file, "w") as outfile:
        lines = infile.readlines()
        line_buffer = []  # holds up to 16 hex strings

        for line in lines:
            if not line.strip():
                continue

            binaries = LineToBinary(line.strip())

            if isinstance(binaries, (tuple, list)):
                for b in binaries:
                    line_buffer.append(BinToHex(b))
                    if len(line_buffer) == 16:
                        outfile.write(" ".join(line_buffer) + "\n")
                        line_buffer = []
            else:
                line_buffer.append(BinToHex(binaries))
                if len(line_buffer) == 16:
                    outfile.write(" ".join(line_buffer) + "\n")
                    line_buffer = []

        # Write remaining values if any
        if line_buffer:
            outfile.write(" ".join(line_buffer))


if __name__ == "__main__":
    Compile("main.pglu", "out.bin")