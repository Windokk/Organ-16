# Potiglu 16
A simple 16-bit CPU architecture and it's assembly-to-machine-code compiler

## Architecture - Overview

16-bit CPU, designed with CMOS physical implementation in mind.

#### Data Width: 16 bits
- All internal data paths, ALU operations, and registers operate on 16-bit wide values.

#### Address Width: 16 bits
- Allows for a total of 64KB of addressable memory space, mapped linearly.

#### Instruction Width: 16 bits per word
- Supports both single-word and multi-word instructions, enabling operations with immediate values or extended addresses.

#### Registers:

- 8 General Purpose Registers (R0–R7), each 16 bits wide.
- 4 Special Purpose Registers:
  - Program Counter (PC) (16-bits) – holds the address of the current or next instruction. 
  - Instruction Register (IR_MAIN and IR_EXT) (16-bits) – stores the currently executing instruction/extension.
  - Stack Pointer (SP) (16-bits) – points to the top of the software-managed stack in RAM. 
  - Status Register (FLAGS) (4-bits) – holds condition flags: Zero (Z), Negative (N), Carry (C), and Overflow (O).

## ISA : 
Each instruction is 16 bits wide but can be extended to 32 bits in exceptional cases.
- The first 3 bits of the first word represent the Opcode.
- The next 4 bits represent the SubOpcode.
- The remaining 9 bits are used for operands, typically register identifiers.
  In certain instructions, a second 16-bit word may follow to hold a memory address or an immediate value.

Here is the complete list of instructions : 

| Instruction (✅ = Fully implemented<br/> in both the logisim circuit and the compiler) | `OpCode` | `SubOpCode` | Format        | Example          | Description                       |
|---------------------------------------------------------------------------------------|----------|-------------|---------------|------------------|-----------------------------------|
| ADD ✅                                                                                 | `000`    | `0000`      | RRR (1 word)  | ADD R1, R2, R3   | Add R2 and R3, store in R1        |
| SUB ✅                                                                                 | `000`    | `0001`      | RRR (1 word)  | SUB R1, R2, R3   | Subtract R3 from R2, store in R1  |
| MUL ✅                                                                                 | `000`    | `0010`      | RRR (1 word)  | MUL R1, R2, R3   | Multiply R2 and R3, store in R1   |
| DIV ✅                                                                                 | `000`    | `0011`      | RRR (1 word)  | DIV R1, R2, R3   | Divide R2 by R3, store in R1      |
| MOD ✅                                                                                 | `000`    | `0100`      | RRR (1 word)  | MOD R1, R2, R3   | Modulo R2 % R3, store in R1       |
| AND ✅                                                                                 | `000`    | `0101`      | RRR (1 word)  | AND R1, R2, R3   | Bitwise AND, store in R1          |
| OR ✅                                                                                  | `000`    | `0110`      | RRR (1 word)  | OR R1, R2, R3    | Bitwise OR, store in R1           |
| NAND ✅                                                                                | `000`    | `0111`      | RRR (1 word)  | NAND R1, R2, R3  | Bitwise NAND, store in R1         |
| NOR ✅                                                                                 | `000`    | `1000`      | RRR (1 word)  | NOR R1, R2, R3   | Bitwise NOR, store in R1          |
| XOR ✅                                                                                 | `000`    | `1001`      | RRR (1 word)  | XOR R1, R2, R3   | Bitwise XOR, store in R1          |
| NOT ✅                                                                                 | `001`    | `1011`      | RR (1 word)   | NOT R1, R2       | Bitwise NOT of R2 into R1         |
| MOV ✅                                                                                 | `010`    | `0000`      | RI (2 words)  | MOV R1, 0x1234   | Move 16-bit immediate into R1     |
| LOAD ✅                                                                                | `011`    | `0000`      | MEM (2 words) | LOAD R1, 0x1000  | Load from memory to R1            |
| STORE ✅                                                                               | `011`    | `0001`      | MEM (2 words) | STORE R1, 0x1000 | Store R1 to memory[Immediate]     |
| STORER ✅                                                                              | `011`    | `0010`      | RR (1 word)   | STORER, R3, R0   | Store R3 at memory[R0]            |
| JMP ✅                                                                                 | `100`    | `0000`      | JMP (2 words) | JMP 0x1000       | Unconditional jump                |
| JE ✅                                                                                  | `100`    | `0001`      | JMP (2 words) | JE 0x1000        | Jump if equal                     |
| JNE ✅                                                                                 | `100`    | `0010`      | JMP (2 words) | JNE 0x1000       | Jump if not equal                 |
| JB ✅                                                                                  | `100`    | `0011`      | JMP (2 words) | JB 0x1000        | Jump if below (unsigned)          |
| JBE ✅                                                                                 | `100`    | `0100`      | JMP (2 words) | JBE 0x1000       | Jump if below or equal (unsigned) |
| JA ✅                                                                                  | `100`    | `0101`      | JMP (2 words) | JA 0x1000        | Jump if above (unsigned)          |
| JAE ✅                                                                                 | `100`    | `0110`      | JMP (2 words) | JAE 0x1000       | Jump if above or equal (unsigned) |
| JL ✅                                                                                  | `100`    | `0111`      | JMP (2 words) | JL 0x1000        | Jump if less (signed)             |
| JLE ✅                                                                                 | `100`    | `1000`      | JMP (2 words) | JLE 0x1000       | Jump if less or equal (signed)    |
| JG ✅                                                                                  | `100`    | `1001`      | JMP (2 words) | JG 0x1000        | Jump if greater (signed)          |
| JGE ✅                                                                                 | `100`    | `1010`      | JMP (2 words) | JGE 0x1000       | Jump if greater or equal (signed) |
| JSR ✅                                                                                 | `100`    | `1011`      | JMP (2 words) | JSR 0x1000       | Push PC, jump to subroutine       |
| RTS ✅                                                                                 | `100`    | `1100`      | JMP (2 words) | RTS              | Pop return address, jump to it    |
| HLT ✅                                                                                 | `111`    | `0000`      | HLT (1 word)  | HLT              | Halt the CPU                      |
| CMP ✅                                                                                 | `001`    | `1010`      | RR (1 word)   | CMP R1, R2       | Compare R1 and R2                 |
| PUSH ✅                                                                                | `101`    | `0000`      | R (1 word)    | PUSH R1          | Push R1 onto the stack            |
| POP ✅                                                                                 | `101`    | `0001`      | R (1 word)    | POP R1           | Pop top of stack into R1          |

### Instruction Formats

| Format | OpCode | Word Count | Structure                                                                          |
|--------|--------|------------|------------------------------------------------------------------------------------|
| RRR    | 000    | 1 word     | [3b opcode][4b sub-op][3b Rdest][3b RB][3b RC]                                     |
| RR     | 001    | 1 word     | [3b opcode][4b sub-op][3b unused][3b RA][3b RB]                                    |
| RI     | 010    | 2 words    | Word 1: [3b opcode][4b sub-op][3b Rdest][6b unused]  <br> Word 2: 16-bit immediate |
| MEM    | 011    | 2 words    | Word 1: [3b opcode][4b sub-op][3b Rdest][6b unused]  <br> Word 2: 16-bit address   |
| JMP    | 100    | 2 words    | Word 1: [3b opcode][4b sub-op][9b unused]          <br> Word 2: 16-bit address     |
| R      | 101    | 1 word     | [3b opcode][4b sub-op][3b R1][6b unused]                                           |
| HLT    | 111    | 1 word     | [3b opcode][4b sub-op][9b unused]                                                  |

### Future plans :

- [ ] IRL breadboard implementation
- [ ] Support for operations and comparisons between memory-registers and memory-memory
- [ ] PCB version
- [ ] 32 bits ??