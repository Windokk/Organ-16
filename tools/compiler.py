import os
import re
import sys
import json
from typing import Tuple, List, Optional

STACK_START = 0xF000
STACK_END = 0xFFFF
MAX_MEMORY = 0x10000

def is_stack_overlap(start_addr: int, instr_count: int) -> bool:
    return start_addr < STACK_END and (start_addr + instr_count) > STACK_START

def replace_names_with_values(expr: str, mapping: dict) -> str:
    for name in sorted(mapping.keys(), key=len, reverse=True):
        pattern = re.compile(r'\b' + re.escape(name) + r'\b', flags=re.IGNORECASE)
        new_expr = pattern.sub(str(mapping[name]), expr)
        expr = new_expr
    return expr

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
        print(f"File '{filename}' not found.", file=sys.stderr)
        sys.exit(1)

def ImmediateToBin(immediate: str, label_map: dict = None, constants_map: dict = None) -> str:
    try:

        expr = immediate

        if constants_map:
            expr = replace_names_with_values(expr, constants_map)

        if label_map:
            expr = replace_names_with_values(expr, label_map)


        # Evaluate expression safely, only allow integer operations
        value = eval(expr, {"__builtins__": None}, {})

        if isinstance(value, float):
            if value.is_integer():
                value = int(value)
            else:
                raise ValueError(...)
        elif not isinstance(value, int):
            raise ValueError(...)

        return bin(value & 0xFFFF)[2:].zfill(16)

    except Exception as e:
        raise ValueError(f"Invalid immediate value or expression: {immediate} — {e}")

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
        "POP":  ("101", "0001"), "STORER": ("011", "0010")
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
    # Tokenize by splitting on commas or spaces
    tokens = [tok.strip() for tok in re.split(r'[,\s]+', operands) if tok.strip()]
    # Extract registers
    regs = [tok.upper() for tok in tokens if re.fullmatch(r"R[0-7]", tok.upper())]
    # Remove registers from tokens, rest is the immediate
    non_regs = [tok for tok in tokens if tok.upper() not in regs]
    # If there's an immediate expression (e.g., CONSTANT / 2), join it
    imm = " ".join(non_regs) if non_regs else None
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

def LineToBinary(line: str, label_map: dict = None, constants_map: dict = None):
    instr, regs, imm = ParseLine(line)
    if not instr:
        return None
    op_bin = OpCodeToBinOpCode(instr)
    if len(regs) == 0:
        if imm:
            return op_bin + "000000000", ImmediateToBin(imm, label_map, constants_map)
        return op_bin + "000000000"
    elif len(regs) == 1:
        if imm:
            if op_bin == "0110001":
                return op_bin + "000" + DataToBinData(regs) + "000", ImmediateToBin(imm, label_map, constants_map)
            else:
                return op_bin + DataToBinData(regs) + "000000", ImmediateToBin(imm, label_map, constants_map)
        if op_bin == "1010001":
            return op_bin + DataToBinData(regs) + "000000"
        else:
            return op_bin + "000" + DataToBinData(regs) + "000"
    elif len(regs) == 2:
        if op_bin == "0011010" or op_bin == "0110010":
            return op_bin + "000" + DataToBinData(regs)
        else:
            return op_bin + DataToBinData(regs) + "000"
    elif len(regs) == 3:
        return op_bin + DataToBinData(regs)

def BinToHex(binary: str) -> str:
    return hex(int(binary, 2))[2:].zfill(4).lower()

def ParseConstants(lines: List[str]) -> dict:
    """
    Extract constants from lines starting with '@define CONSTANT VALUE'
    Supports referencing previously defined constants.
    Returns a dict of {CONSTANT_NAME: int_value}
    """
    constants = {}
    for line in lines:
        line = line.strip()
        if line.startswith("@define"):
            parts = line.split(maxsplit=2)
            if len(parts) == 3:
                _, name, value = parts
                value_upper = value.upper()
                try:
                    if value_lower := value.lower():
                        if value_lower.startswith("0x"):
                            val = int(value_lower, 16)
                        elif value_lower.startswith("0b"):
                            val = int(value_lower, 2)
                        elif value_upper in constants:
                            val = constants[value_upper]
                        else:
                            val = int(value_lower, 10)
                    constants[name.upper()] = val
                except ValueError:
                    print(f"Warning: Invalid constant value '{value}' for {name}, ignoring.")
            else:
                print(f"Warning: Invalid @define syntax: '{line}'")
    return constants

def ReplaceLabelsAndConstants(line: str, label_map: dict, constants_map: dict) -> str:
    """
    Replace label or constant names in immediate fields with their numeric values.
    """
    instr, regs, imm = ParseLine(line)
    if imm:
        imm_upper = imm.upper()
        if imm_upper in label_map:
            line = line.replace(imm, f"0x{label_map[imm_upper]:04X}")
        elif imm_upper in constants_map:
            line = line.replace(imm, str(constants_map[imm_upper]))
    return line

def CompileMultiple(segments: List[Tuple[str, int]], output_file: str):
    memory = ["0000"] * 65536  # 64K words initialized to zero

    global_labels = {}
    file_lines = []
    all_constants = {}

    for filepath, base_addr in segments:
        lines = ReadFileLines(filepath)

        # Extract constants first
        constants = ParseConstants(lines)
        all_constants.update(constants)

        # Remove @define lines before further processing
        lines = [line for line in lines if not line.strip().startswith("@define")]

        instr_count = sum(InstructionWordCount(line) for line in lines if not IsLabel(line))
        max_instr = STACK_START - base_addr

        if base_addr >= STACK_START:
            print(f"Warning: Skipping segment '{filepath}' - starts in stack memory (0x{base_addr:04X}).")
            continue

        if instr_count > max_instr:
            print(f"Cropping segment '{filepath}' - {instr_count} words reduced to {max_instr} to avoid stack memory overlap.")
            new_lines = []
            words_used = 0
            for line in lines:
                if IsLabel(line):
                    new_lines.append(line)
                    continue
                word_count = InstructionWordCount(line)
                if words_used + word_count > max_instr:
                    break
                new_lines.append(line)
                words_used += word_count
            lines = new_lines

        label_map = FirstPass(lines, base_addr)
        global_labels.update(label_map)
        file_lines.append((lines, base_addr, constants))  # pass constants too

    for lines, base_addr, constants in file_lines:
        addr = base_addr
        for line in lines:
            if IsLabel(line):
                continue
            # Replace labels and constants
            line = ReplaceLabelsAndConstants(line, global_labels, constants)
            binaries = LineToBinary(line, global_labels, constants)
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

def find_first_list(data):
    if isinstance(data, list):
        return data
    elif isinstance(data, dict):
        for value in data.values():
            result = find_first_list(value)
            if result is not None:
                return result
    return None

def main():
    if len(sys.argv) < 2:
        print("Usage: python script.py <filename.l>")
        return

    filename = sys.argv[1]
    linker_dir = os.path.dirname(os.path.abspath(filename))

    if not filename.endswith('.l'):
        print("Error: File must be a linker (have a .l extension).", file=sys.stderr)
        sys.exit(1)

    if not os.path.exists(filename):
        print(f"Error: File '{filename}' does not exist.", file=sys.stderr)
        sys.exit(1)

    try:
        with open(filename, 'r') as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        print(f"Error parsing JSON: {e}", file=sys.stderr)
        sys.exit(1)

    lst = find_first_list(data)
    segments = []

    if lst is None:
        print("No list found in the JSON data.")
        return

    for item in lst:
        if isinstance(item, dict) and len(item) == 1:
            key, value = next(iter(item.items()))
            base = int(value, 16)

            if base >= 0xF000:
                print(f"Warning: Segment '{key}' starts in stack memory at {value}, skipping.")
                continue
            
            segment_path = os.path.join(linker_dir, key)
            segments.append((segment_path, base))
        else:
            print(f"Skipping item (not a single-pair dict): {item}")

    if not segments:
        print("No valid segments to compile.", file=sys.stderr)
        sys.exit(1)

    CompileMultiple(segments, filename[:-1] + "bin")

    print(f"Successfully compiled {filename[:-1]}bin")

if __name__ == '__main__':
    main()