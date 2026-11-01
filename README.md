<div align="center">
  
  # Organ 16

  <img src="LogoGithub.png" width="30%">

  </br>
  
  <img src="https://img.shields.io/static/v1?label=Version&message=2.5.0&color=33cc33&style=for-the-badge"/>


</br>
</br>

A "simple" 16-bit CPU architecture and its assembly-to-machine-code compiler + emulator

Designed with CMOS IRL implementation in mind (on breadboard for now)

</div>

</br>

<img src="Screenshot.png"/>
<div align="center"><i>Emulator playing pong.bin program</i></div>
</br>
<div align="center"><img src="organ16_cube.gif" width="50%"></br>
<i>Spinning cube (Rendered in emulator)</i></div>

## Repository Guide

- [asm-lang/](asm-lang/)  : contains extension(s) for common IDEs to support Organ16 assembly
- [circuits/](circuits/)  : contains every logisim (evolution) related files
- [docs/](docs/)          : documentation on the whole project (logisim, emulator, asm, irl implementation)
- [emulator/](emulator/)  : the source code for the emulator (written in C++ with Qt). Put the B612 `.ttf` fonts in `emulator/src/fonts/` before building
- [programs/](programs/)  : various test/demo programs to be executed on the emulator/on logisim
- [tools/](tools/)        : various tools used for developping and building the CPU and its extensions

## Architecture - Overview

16-bit CPU, designed with CMOS physical implementation in mind.

#### Data Width: 16 bits

- All internal data paths, ALU operations, and registers operate on 16-bit wide values.

#### Address Width: 16 bits

- Allows for a total of 1Mbit of addressable memory space, mapped linearly.

#### Instruction Width: 16 bits per word

- Supports both single-word and multi-word instructions, enabling operations with immediate values or extended addresses.

#### 8KB Stack

- Supports a very basic stack pointer and associated instructions such as POP, PUSH, etc...
- The stack's base size is 8KB (in future updates, there will be ways to augment/reduce it)
- Stack pointer's reset address is 0xffff (end of memory)

#### Registers:

- 8 General Purpose Registers (R0–R7), each 16 bits wide.
- 4 Special Purpose Registers:
  - Program Counter (PC) (16-bits) – holds the address of the current or next instruction. 
  - Instruction Register (IR_MAIN and IR_EXT) (16-bits) – stores the currently executing instruction/extension.
  - Stack Pointer (SP) (16-bits) – points to the top of the software-managed stack in RAM. 
  - Status Register (FLAGS) (4-bits) – holds condition flags: Zero (Z), Negative (N), Carry (C), and Overflow (O).

## ISA : CISC

(Currently there are 59 instructions, but by design the CPU could support 128 instructions)
Each instruction is 16 bits wide but can be extended to 32 bits in some cases.
- The first 3 bits of the first word represent the Opcode.
- The next 4 bits represent the SubOpcode.
- The remaining 9 bits are used for operands, typically register identifiers.
  In certain instructions, a second 16-bit word may follow to hold a memory address or an immediate value.

### File format/ASM Compiler :

The only file extensions recognized by the compiler are ".l" linker scripts and .org asm source files
The asm compiler is implemented in Python 3.10.
To compile a source file, first create a .l linker script and fill it with segments : 
(using json synthax)

```json
{
    "segments": [ 
        {"SourceFile.org": "0x0000"},
        {"SourceFile2.org": "0x3000"},
    ]
}
```

* Source files should be in the same directory as the linker script

Compiler will then create a .bin file (with the same name as the linker script)

It will place every source file at the memory address specified in the linker script.

(Memory Address shall be calculated so that the programs data doesn't overlap the stack (0xf000 to 0xffff))

(For example, if a program takes three words and is placed at 0xefff, its content will be cropped to let enough space for the stack) 

The project also features a (WIP) Mini-C compiler (subset of the C programming language).

## More details can be found in the docs/ folder ! 


### Known issues/Missing features:

- Support for signed operations is not enforced; it is up to the program to choose whether to work with signed or unsigned numbers.

### Actual version : 2.5.0 :
- [x] Mini-C compiler : occ (Organ C Compiler) v.0.2
- [x] New emulator style
- [x] Instructions reader (in emulator)


### Future plans : v3.0.0 :
- [ ] ISA Enhancements :
  - [ ] ALU-immediate group : ADDI, SUBI, MULI, ANDI, ORI, XORI, SHLI, SHRI, MODI, DIVI, NANDI, NORI, NOTI, XNORI
  - [ ] LOADO + STOREO (replaces MOVI base + ADD + LOADR) : allows for : array[i] in two words (instead of 4)
  - [ ] LOADRD + STORERD (store register at register held adress then decrement held adress)
  - [ ] Relative short jump : PC = PC + 8 bit signed offset (9 bits offset total)
    - [ ] BRA
    - [ ] BE
    - [ ] BNE
    - [ ] BL
    - [ ] BLE
    - [ ] BB
    - [ ] BBE
    - [ ] BA
    - [ ] BAE
    - [ ] BG
    - [ ] BGE
    - [ ] BSR
  - [ ] SAR, SDIV, SMOD
  - [ ] NEG (negate)
  - [ ] Allow access to SP and PC as a registers
  - [ ] JSRR, JMPR, JER, JNER, JBR, JBER, JAR, JAER, JLR, JLER, JGR, JGER (jump to register held adresses)
- [ ] Mini-C compiler : occ v.0.5
- [ ] Interrupts
- [ ] Exceptions (a kind of interrupt)
- [ ] Cache

### Future-future plans (v4.0.0?)

- [ ] 32 bit evolution (Carillon 32) --> this means abandonning circuit simulation in Logisim Evolution (max = 24 bit adress space)
- [ ] Peripherals support : sound card, different screens sizes, keyboard, etc...
- [ ] Multi-core
- [ ] Doom port
- [ ] IRL breadboard implementation (???)
- [ ] PCB version (In a long long time 😄)