# Organ 16 C (Mini-C) - Compiler specification v0.1

`occ.py` translates a minimal subset of C into Organ 16 assembly (`.org`), then hands it to the existing assembler.
This document is the contract for v0.1. Anything not listed here is rejected with a diagnostic.

## Target facts the design relies on

- Memory is word-addressed (16-bit words). A pointer is a word address, `p + 1` is the next word, there is no scaling and no `char` type.
- Memory map: code `0x0000-0x7FFF`, framebuffer `0x8000-0xBFFF` (128x128, one word per pixel), data `0xC000-0xEFFF`, stack `0xF000-0xFFFF`.
- Registers R0-R7 are the only general registers. `SP` cannot be used as an operand, so the hardware stack is only reachable through `PUSH`, `POP`, `JSR`, `RTS`.
- `DIV`, `MOD`, `SHR`, `MIN`, `MAX` are unsigned. `SHR` is logical. `MUL` returns the low 16 bits (correct for signed too).
- `JL/JLE/JG/JGE` are signed, `JB/JBE/JA/JAE` are unsigned. Jumps take an absolute 2-word target. There is no conditional jump on a register: a `CMP`, `COMPI` or `COMPM` always precedes.
- `INC`, `DEC`, `LOADRP`, `STORERP` exist (see `docs/ORG 16 ASM/org16 assembly.md`).

## Command line and output

```
python tools/occ.py prog.c [-o outdir] [--no-assemble]
```

| File | Content |
|---|---|
| `prog.org` | generated assembly (code, crt0, runtime routines that are used) |
| `prog.data.bin` | globals and string literals, ASCII hex words, one segment at `0xC000`; trailing zero words are dropped and the file (and its segment) is omitted if everything is zero |
| `prog.l` | generated linker script: `prog.org` at `0x0000`, `prog.data.bin` at `0xC000` |
| `prog.bin` | final image, produced by invoking the assembler on `prog.l` (skipped with `--no-assemble`) |

Diagnostics are printed as `file:line: error: message`. Compilation stops at the first error and the exit code is non-zero. Warnings (`file:line: warning: message`) do not stop it: incompatible pointer types, discarded `const`, constant division by zero.

## Language

### Types

| Type | Meaning |
|---|---|
| `int` | 16-bit signed (default signedness) |
| `unsigned` | 16-bit unsigned |
| `void` | function return type only |
| `T*` | pointer to `int` or `unsigned` (16-bit word address) |
| `T x[N]` | one-dimensional array of `int` or `unsigned`; `N` is a constant expression, or omitted when an initializer gives the size |

Rejected: `char`, `short`, `long`, `float`, `double`, `struct`, `union`, `enum`, `typedef`, multi-dimensional arrays, `void*`, function pointers, `static`, `extern`, `volatile`.
`const` is accepted and enforced (assignment to a const object is an error).

### Constants

Decimal, hexadecimal (`0x`), binary (`0b`), and character literals (`'a'`, value is the ASCII code, type `int`).
String literals `"abc"`: see 3.6.

### Expressions

Operators: `+ - * / % & | ^ ~ << >> ! && || == != < <= > >=`, assignment `= += -= *= /= %= &= |= ^= <<= >>=`, `++ --` (prefix and postfix), `?:`, unary `-`, `&x`, `*p`, `p[i]`, calls, casts `(T)e`, parentheses.
Not supported: comma operator, `sizeof`.

Rules:

- Arithmetic is modulo 2^16. Overflow is not diagnosed.
- If either operand is `unsigned` (or a pointer), the operation is unsigned, otherwise signed.
- `&&` and `||` short-circuit. Operands of calls are evaluated left to right.
- Pointer arithmetic is not scaled. `p + n`, `p - n`, `p - q` and comparisons are allowed. Pointer comparisons are unsigned.
- Arrays decay to a pointer to their first element. `int a[]` as a parameter means `int*`.
- A cast from an integer constant to a pointer is allowed. It is the way to reach fixed addresses:

```c
#define FB ((unsigned*)0x8000)
FB[y * 128 + x] = 0xFFE0;
```

Behaviour that differs from or refines ISO C:

- `>>` on a signed `int` is an arithmetic shift (runtime routine `__sar`, the hardware only has the logical `SHR`).
- `/` and `%` by 0 give 0 (hardware behaviour).

### Statements

`if/else`, `while`, `do/while`, `for`, `break`, `continue`, `return`, blocks, expression statements, declarations.
Not supported: `switch`, `goto`, labels.

### Declarations and functions

- Globals and locals of the types in 3.1. A forward declaration (prototype) is allowed and required before use if the callee is defined later.
- Global initializers must be constant expressions: `int x = 3;`, `unsigned t[4] = {1, 2, 3, 4};`, `int s[] = "abc";`.
- Local scalar initializers are allowed and run each time the declaration is reached. Local array initializers are not supported in v0.1 (use a global).
- Uninitialized globals are zero. Locals are indeterminate.
- Functions take 0 to 4 parameters and return `int`, `unsigned` or `void`. Variadic functions are rejected.
- The entry point is `main`, with no parameters. It may return `void` or `int`; the value is ignored.
- Recursion, direct or indirect, is an error (see 4.1).

### String literals

`"abc"` is an array of words, one character per word (ASCII code), terminated by a 0 word. Escapes: `\n \t \0 \\ \" \'`.

- `int s[] = "abc";` initializes a global array of size 4.
- `int* p = "abc";` points at an anonymous array placed in the data segment. Identical literals are shared.
- Literals are placed among the globals, in order of first appearance, in `prog.data.bin`.

### Preprocessor

`#define NAME tokens` (object-like only, replaced textually, may reference earlier macros). Comments `//` and `/* */`.
Not supported: `#include`, `#ifdef`, `#if`, macros with arguments.

### Intrinsics

Calls to these names compile directly to one instruction. They are predeclared and cannot be redefined.

| Intrinsic | Instruction | Notes |
|---|---|---|
| `unsigned in0()`, `in1()`, `in2()` | `IN0/IN1/IN2` | |
| `void out0(unsigned v)`, `out1`, `out2` | `OUT0/OUT1/OUT2` | |
| `unsigned rand()` | `RAND` | |
| `int sin(int a)`, `int cos(int a)` | `SIN`, `COS` | angle in turns (`0x4000` = 90 degrees), result Q1.15 |
| `int tan(int a)` | `TAN` | result Q8.8, saturated |
| `unsigned sqrt(unsigned x)` | `SQRT` | floor |
| `unsigned pow(unsigned a, unsigned b)` | `POW` | low 16 bits |
| `int abs(int x)` | `ABS` | `abs(0x8000) == 0x8000` |
| `unsigned min(unsigned a, unsigned b)`, `max` | `MIN`, `MAX` | unsigned |
| `void halt()` | `HLT` | |

## Code generation model

### Static allocation of locals

Parameters and local variables of a function get fixed addresses in the data region, decided at compile time. There is no run-time frame.

- Layout: globals and string literals grow upward from `0xC000` in order of appearance (they are the content of `prog.data.bin`). Function parameters and locals grow downward from `0xEFFF`. The two areas meeting is an error.
- Access to a variable is `LOAD Rd, addr` / `STORE Rs, addr`.
- A function entry copies its register arguments into its parameter slots.
- The compiler builds the call graph. Any cycle is an error: `error: recursion f -> g -> f`. Calling `f(f(1))` from another function is valid, only cycles in the graph matter.
- Functions that are never active at the same time share addresses. The compiler parses the program twice: the first pass measures each frame, then each function gets an offset equal to the largest `offset + size` among its callers (`main` at 0), so a function never overlaps any of its direct or indirect callers or callees, while siblings (`f` and `g` both called by `h`) overlap freely. Functions unreachable from `main` are not placed.
- The overflow of the data region (`> 0xEFFF`) is an error; it is checked against the shared layout.

`&x` of a local gives its static address. Using it after the function returned is undefined.

### Registers and calls

| Registers | Role |
|---|---|
| R0-R3 | arguments (in order), R0 also holds the return value |
| R0-R5 | evaluation stack for expression temporaries; on exhaustion the compiler spills with `PUSH`/`POP` |
| R6-R7 | reserved for the compiler (addresses, pointer temporaries) |

- All registers are caller-saved. Before a `JSR`, the caller `PUSH`es the live temporaries and `POP`s them afterwards.
- The program starts with crt0: `JSR main` then `HLT`.

### Instruction selection

| C | Assembly |
|---|---|
| `x++`, `x--`, `x + 1`, `x - 1` | `INC` / `DEC` |
| `v = *p++`, `*p++ = v` | `LOADRP` / `STORERP` |
| `a[i]`, `*p` | address in a register (`ADD`), then `LOADR` / `STORER` |
| global or local `x` | `LOAD` / `STORE` with the static address |
| `r < constant` in a condition | `COMPI`, then the inverse conditional jump |
| `r < global` in a condition | `COMPM`, then the inverse conditional jump |
| `<`, `<=`, `>`, `>=` | `JL/JLE/JG/JGE` if signed, `JB/JBE/JA/JAE` if unsigned |
| `* & \| ^ ~ <<` | `MUL AND OR XOR NOT SHL` |
| `>>` on `unsigned` | `SHR` |
| `/ %` on `unsigned` | `DIV` / `MOD` |
| `/ %` on `int` | call to `__sdiv` / `__smod` |
| `>>` on `int` | call to `__sar` |
| comparison used as a value | `MOVI 1`, compare, conditional jump, `MOVI 0` |

Constant folding is applied. No other optimization is part of v0.1.

### Runtime routines

Emitted into `prog.org` only when used: `__sdiv`, `__smod` (sign handling around `DIV`/`MOD` with `ABS`), `__sar`. They follow the normal calling convention (arguments in R0-R1, result in R0).

## Implementation notes (v0.1)

- Integer literals above `0x7FFF` (decimal or hex) have type `unsigned`; above `0xFFFF` is an error. Octal is rejected. Character escapes: `\n \t \r \0 \\ \' \"`.
- Declarations accept several declarators (`int a = 1, *p, t[4];`), `unsigned int`, `int const`, and a declaration as the first clause of `for`.
- Functions that are not reachable from `main` are type-checked but not emitted.
- `lv op= e` is only accepted when `lv` has no side effects (`a[i++] += 1` is an error; `a[i++]++` is fine).
- An integer other than a constant cannot be compared with a pointer. The constant `0` converts to a null pointer.
- Calls: arguments are passed in R0-R3. Constant and plain-variable arguments are loaded last, directly into their register, when no later argument has a side effect.
- Tests: `python3 tools/occ_tests/run_tests.py` (annotated programs and expected errors, run on the emulator core) and `python3 tools/occ_tests/fuzz.py [count] [first_seed] [max_depth]` (random programs checked against an independent 16-bit reference evaluator). See `tools/occ_tests/README.md`.
