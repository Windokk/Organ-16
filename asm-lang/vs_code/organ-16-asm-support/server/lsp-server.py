import json
import sys
from pygls.server import LanguageServer
from lsprotocol.types import (
    Diagnostic,
    DiagnosticSeverity,
    DidOpenTextDocumentParams,
    DidChangeTextDocumentParams,
    Range,
    Position,
    TEXT_DOCUMENT_DID_OPEN,
    TEXT_DOCUMENT_DID_CHANGE,
    TEXT_DOCUMENT_COMPLETION,
    CompletionParams,
    CompletionItem,
    CompletionItemKind,
    CompletionList,
)

import re

server = LanguageServer("Organ 16 Language", "v0.1")

# === Constants & Helpers ===

VALID_OPCODES = {
    'ADD': ('RRR', 3),
    'SUB': ('RRR', 3),
    'MUL': ('RRR', 3),
    'DIV': ('RRR', 3),
    'MOD': ('RRR', 3),
    'AND': ('RRR', 3),
    'OR': ('RRR', 3),
    'NAND': ('RRR', 3),
    'NOR': ('RRR', 3),
    'XOR': ('RRR', 3),
    'NOT': ('RR', 2),
    'MOV': ('RI', 2),
    'LOAD': ('MEM', 2),
    'LOADR': ('RR', 2),
    'STORE': ('MEM', 2),
    'STORER': ('RR', 2),
    'JMP': ('JMP', 1),
    'JE': ('JMP', 1),
    'JNE': ('JMP', 1),
    'JB': ('JMP', 1),
    'JBE': ('JMP', 1),
    'JA': ('JMP', 1),
    'JAE': ('JMP', 1),
    'JL': ('JMP', 1),
    'JLE': ('JMP', 1),
    'JG': ('JMP', 1),
    'JGE': ('JMP', 1),
    'JSR': ('JMP', 1),
    'RTS': ('JMP', 0),
    'HLT': ('HLT', 0),
    'CMP': ('RR', 2),
    'PUSH': ('R', 1),
    'POP': ('R', 1),
}

VALID_REGISTERS = {f"R{i}" for i in range(8)}

STACK_BASE_START = 0xF000
STACK_BASE_END = 0xFFFF

IMMEDIATE_REGEX = re.compile(r'^(0x[0-9a-fA-F]+|0b[01]+|\d+)$')  # Hex, binary, decimal

# === Utility ===

def make_error(line, start_char, end_char, message):
    return Diagnostic(
        range=Range(
            start=Position(line=line, character=start_char),
            end=Position(line=line, character=end_char)
        ),
        message=message,
        severity=DiagnosticSeverity.Error,
        source="Organ 16 Language"
    )

def is_immediate(token, defined_constants):
    token = token.strip()
    # if its an integer
    if re.match(r'^-?\d+$', token) or re.match(r'0b[01]+', token) or re.match(r'0x[0-9A-Fa-f]+', token) or token in defined_constants:
        return True
    # If its between parentheses
    if token.startswith('(') and token.endswith(')'):
        return is_valid_expr(token, defined_constants)
    return False

def split_operands(operand_str):
    operands = []
    current = ''
    depth = 0

    for char in operand_str:
        if char == ',' and depth == 0:
            operands.append(current.strip())
            current = ''
        else:
            if char == '(':
                depth += 1
            elif char == ')':
                depth -= 1
            current += char

    if current.strip():
        operands.append(current.strip())

    return operands

def is_valid_expr(expr, constants=None):
    expr = expr.strip()
    constants = constants or {}

    # Si c'est un entier pur
    if re.match(r'^-?\d+$', expr):
        return True

    # Remplace les constantes dans l'expression
    try:
        safe_expr = expr
        safe_expr = re.sub(r'0x[0-9A-Fa-f]+', lambda m: str(int(m.group(), 16)), safe_expr)
        safe_expr = re.sub(r'0b[01]+', lambda m: str(int(m.group(), 2)), safe_expr)
        
        for name, value in constants.items():
            safe_expr = re.sub(rf'\b{re.escape(name)}\b', str(value), safe_expr)

        # Évalue l'expression mathématique
        eval_result = eval(safe_expr, {"__builtins__": None}, {})
        return isinstance(eval_result, (int, float))
    except Exception:
        return False

# === Assembly Source Validation ===

def validate_org_source(lines):
    diagnostics = []
    labels_defined = {}
    labels_used = set()
    defined_constants = {}
    constants_seen = set()

    # 1st pass : definitions
    for lineno, line in enumerate(lines):
        code = line.split('#')[0].split(';')[0].strip()
        if not code:
            continue

        # Collecte des constantes (juste le nom)
        if code.startswith("@define"):
            tokens = code.split()
            if len(tokens) == 3:
                _, name, value = tokens
                if re.match(r'^[A-Za-z_][A-Za-z0-9_]*$', name):
                    constants_seen.add(name)
            continue

        # Collecte des labels
        if ':' in code:
            label_part = code.split(':', 1)[0].strip()
            if re.match(r'^[A-Za-z_][A-Za-z0-9_]*$', label_part):
                if label_part not in labels_defined:
                    labels_defined[label_part] = lineno
            continue

    # 2nd pass : actual diagnostic
    for lineno, line in enumerate(lines):
        code = line.split('#')[0].split(';')[0].strip()
        if not code:
            continue

        if code.startswith("@define"):
            tokens = code.split()
            if len(tokens) != 3:
                diagnostics.append(make_error(lineno, 0, len(code), "Invalid @define format. Use: @define NAME VALUE"))
                continue

            _, name, value = tokens
            if not re.match(r'^[A-Za-z_][A-Za-z0-9_]*$', name):
                diagnostics.append(make_error(lineno, 0, len(name), f"Invalid constant name '{name}'"))
                continue
            if not is_immediate(value, defined_constants):
                diagnostics.append(make_error(
                    lineno,
                    code.find(value),
                    code.find(value) + len(value),
                    f"Invalid value '{value}' for @define."
                ))
                continue
            if name in defined_constants:
                diagnostics.append(make_error(lineno, 0, len(name), f"Constant '{name}' already defined"))
                continue

            defined_constants[name] = value
            continue

        # Labels
        if ':' in code:
            label_part, rest = code.split(':', 1)
            label = label_part.strip()
            if not re.match(r'^[A-Za-z_][A-Za-z0-9_]*$', label):
                diagnostics.append(make_error(lineno, 0, len(label), f"Invalid label name '{label}'"))
            elif labels_defined[label] != lineno:
                diagnostics.append(make_error(lineno, 0, len(label), f"Duplicate label '{label}'"))
            code = rest.strip()
            if not code:
                continue

        parts = code.split(None, 1)  # Max split into: instr + operands
        
        instr = parts[0].upper()
        if instr not in VALID_OPCODES:
            diagnostics.append(make_error(lineno, line.find(instr), len(line), f"Unknown instruction '{instr}'"))
            continue

        fmt, expected_operands = VALID_OPCODES[instr]

        operands = []
        if len(parts) > 1:
            operands = split_operands(parts[1])

        if len(operands) != expected_operands:
            diagnostics.append(make_error(
                lineno,
                line.find(instr),
                len(line),
                f"Instruction '{instr}' expects {expected_operands} operands, got {len(operands)}"
            ))
            continue


        def is_invalid_register(token):
            return token.upper().startswith('R') and token.upper() not in VALID_REGISTERS

        def is_valid_operand_immediate_or_constant(token):
            return is_immediate(token, defined_constants) or token in defined_constants

        # === Validation spécifique selon format ===

        if fmt in ('RRR', 'RR'):
            for op in operands:
                if is_invalid_register(op):
                    diagnostics.append(make_error(lineno, line.find(op), line.find(op) + len(op), f"Invalid register '{op}'"))
        elif fmt == 'RI':
            if is_invalid_register(operands[0]):
                diagnostics.append(make_error(lineno, line.find(operands[0]), line.find(operands[0]) + len(operands[0]), f"Invalid register '{operands[0]}'"))
            if not is_valid_operand_immediate_or_constant(operands[1]):
                diagnostics.append(make_error(lineno, line.find(operands[1]), line.find(operands[1]) + len(operands[1]), f"Invalid immediate or constant '{operands[1]}'"))
        elif fmt == 'MEM':
            if is_invalid_register(operands[0]):
                diagnostics.append(make_error(lineno, line.find(operands[0]), line.find(operands[0]) + len(operands[0]), f"Invalid register '{operands[0]}'"))
            if not is_valid_operand_immediate_or_constant(operands[1]):
                diagnostics.append(make_error(lineno, line.find(operands[1]), line.find(operands[1]) + len(operands[1]), f"Invalid memory address or constant '{operands[1]}'"))
        elif fmt == 'JMP':
            if expected_operands == 1:
                op = operands[0]
                if not is_immediate(op, defined_constants) and op not in labels_defined and op not in defined_constants:
                    labels_used.add((op, lineno, line.find(op), line.find(op) + len(op)))
        elif fmt == 'R':
            if is_invalid_register(operands[0]):
                diagnostics.append(make_error(lineno, line.find(operands[0]), line.find(operands[0]) + len(operands[0]), f"Invalid register '{operands[0]}'"))

        # === Enregistrement des labels/constants utilisés
        for op in operands:
            if is_invalid_register(op) or op.upper() in VALID_REGISTERS or is_immediate(op, defined_constants):
                continue
            if op not in labels_defined and op not in defined_constants:
                pos = line.find(op)
                labels_used.add((op, lineno, pos, pos + len(op)))

    # Final verification (labels/constants)
    for label, lineno, start, end in labels_used:
        if label not in labels_defined and label not in defined_constants:

            diagnostics.append(make_error(lineno, start, end, f"Undefined label or constant referenced: '{label}'"))

    return diagnostics


@server.feature(TEXT_DOCUMENT_DID_CHANGE)
def on_change(ls: LanguageServer, params: DidChangeTextDocumentParams):
    text = ls.workspace.get_text_document(params.text_document.uri).source
    lines = text.splitlines()
    asm_diags = validate_org_source(lines)
    ls.publish_diagnostics(params.text_document.uri, asm_diags)

@server.feature(TEXT_DOCUMENT_DID_OPEN)
def on_open(ls: LanguageServer, params: DidOpenTextDocumentParams):
    text = ls.workspace.get_text_document(params.text_document.uri).source
    lines = text.splitlines()
    asm_diags = validate_org_source(lines)
    ls.publish_diagnostics(params.text_document.uri, asm_diags)

@server.feature(TEXT_DOCUMENT_COMPLETION)
def completions(ls: LanguageServer, params: CompletionParams):
    # Extract current document and position
    doc = ls.workspace.get_text_document(params.text_document.uri)
    line = doc.lines[params.position.line]
    prefix = line[: params.position.character]

    labels = set()
    constants = set()

    for l in doc.lines:
        code = l.split('#')[0].split(';')[0].strip()

        # Extract labels (e.g., loop:)
        if ':' in code:
            label = code.split(':', 1)[0].strip()
            if re.match(r'^[A-Za-z_][A-Za-z0-9_]*$', label):
                labels.add(label)

        # Extract @define constants
        if code.lower().startswith('@define'):
            tokens = code.split()
            if len(tokens) == 3:
                _, const_name, const_value = tokens
                if re.match(r'^[A-Za-z_][A-Za-z0-9_]*$', const_name):
                    constants.add(const_name)

    # Build completion items for opcodes
    opcode_items = [
        CompletionItem(
            label=op,
            kind=CompletionItemKind.Keyword,
            detail="Opcode",
            insert_text=op,
        )
        for op in VALID_OPCODES.keys()
    ]

    # Build completion items for registers
    register_items = [
        CompletionItem(
            label=reg,
            kind=CompletionItemKind.Variable,
            detail="Register",
            insert_text=reg,
        )
        for reg in VALID_REGISTERS
    ]

    # Build completion items for labels
    label_items = [
        CompletionItem(
            label=lbl,
            kind=CompletionItemKind.Reference,
            detail="Label",
            insert_text=lbl,
        )
        for lbl in labels
    ]

    # Build completion items for constants
    constant_items = [
        CompletionItem(
            label=const,
            kind=CompletionItemKind.Constant,
            detail="Constant (@define)",
            insert_text=const,
        )
        for const in constants
    ]

    # Return all completion items
    all_items = opcode_items + register_items + label_items + constant_items

    return CompletionList(is_incomplete=False, items=all_items)

if __name__ == '__main__':
    server.start_io()