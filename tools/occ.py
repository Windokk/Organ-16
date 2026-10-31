# occ.py : Organ C Compiler
# Compiles a minimal subset of c code to organ 16 asm then to binary

#!/usr/bin/env python3
import argparse
import os
import re
import subprocess
import sys

DATA_BASE = 0xC000
FRAME_TOP = 0xF000
CODE_LIMIT = 0x8000
M = 0xFFFF
TMP = 6
TMP2 = 7
POOL = 6


class CompileError(Exception):
    def __init__(self, msg, line=0):
        super().__init__(msg)
        self.msg = msg
        self.line = line


TOKEN_RE = re.compile(r'''
    (?P<ws>[ \t\r\f\v]+)
  | (?P<nl>\n)
  | (?P<lc>//[^\n]*)
  | (?P<bc>/\*.*?\*/)
  | (?P<ubc>/\*)
  | (?P<num>0[xX][0-9a-fA-F]+|0[bB][01]+|[0-9]+)
  | (?P<id>[A-Za-z_][A-Za-z0-9_]*)
  | (?P<str>"(?:\\.|[^"\\\n])*")
  | (?P<chr>'(?:\\.|[^'\\\n])')
  | (?P<pp>\#)
  | (?P<p><<=|>>=|\+\+|--|\+=|-=|\*=|/=|%=|&=|\|=|\^=|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^~!<>=?:;,(){}\[\]])
''', re.X | re.S)

ESCAPES = {'n': 10, 't': 9, 'r': 13, '0': 0, '\\': 92, "'": 39, '"': 34}

TYPE_KW = {'int', 'unsigned', 'void', 'const'}
TYPE_START = TYPE_KW | {
    'char', 'short', 'long', 'float', 'double', 'struct', 'union', 'enum',
    'signed', 'static', 'extern', 'volatile', 'typedef',
}
STMT_KW = {'if', 'else', 'while', 'do', 'for', 'return', 'break', 'continue'}
UNSUPPORTED = {
    'char', 'short', 'long', 'float', 'double', 'struct', 'union', 'enum', 'typedef',
    'static', 'extern', 'volatile', 'switch', 'case', 'default', 'goto', 'sizeof',
    'signed', 'register', 'auto',
}
ASSIGN_OPS = {'=', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=', '>>='}
BINPREC = {
    '||': 1, '&&': 2, '|': 3, '^': 4, '&': 5, '==': 6, '!=': 6,
    '<': 7, '<=': 7, '>': 7, '>=': 7, '<<': 8, '>>': 8,
    '+': 9, '-': 9, '*': 10, '/': 10, '%': 10,
}
CMP_OPS = {'==', '!=', '<', '<=', '>', '>='}
ARITH_ALU = {'+': 'ADD', '-': 'SUB', '*': 'MUL', '&': 'AND', '|': 'OR', '^': 'XOR', '<<': 'SHL'}
COMMUTATIVE = {'+', '*', '&', '|', '^'}
MIRROR = {'==': '==', '!=': '!=', '<': '>', '<=': '>=', '>': '<', '>=': '<='}
INVERT = {'==': '!=', '!=': '==', '<': '>=', '<=': '>', '>': '<=', '>=': '<'}
JCC_SIGNED = {'==': 'JE', '!=': 'JNE', '<': 'JL', '<=': 'JLE', '>': 'JG', '>=': 'JGE'}
JCC_UNSIGNED = {'==': 'JE', '!=': 'JNE', '<': 'JB', '<=': 'JBE', '>': 'JA', '>=': 'JAE'}


def s16(v):
    v &= M
    return v - 0x10000 if v & 0x8000 else v


def lex(src):
    toks = []
    pos = 0
    line = 1
    while pos < len(src):
        m = TOKEN_RE.match(src, pos)
        if not m:
            raise CompileError(f"unexpected character {src[pos]!r}", line)
        kind = m.lastgroup
        text = m.group()
        start = pos
        pos = m.end()
        if kind == 'ubc':
            raise CompileError("unterminated comment", line)
        if kind in ('ws', 'lc'):
            continue
        if kind == 'bc':
            line += text.count('\n')
            continue
        if kind == 'nl':
            toks.append(('nl', text, line, start))
            line += 1
            continue
        toks.append((kind, text, line, start))
    return toks


def preprocess(toks):
    macros = {}
    out = []
    i = 0
    at_start = True
    last_line = 1

    def expand(tok, hide):
        kind, val, line, pos = tok
        if kind == 'id' and val in macros and val not in hide:
            res = []
            for t in macros[val]:
                res.extend(expand((t[0], t[1], line, t[3]), hide | {val}))
            return res
        return [tok]

    while i < len(toks):
        kind, val, line, pos = toks[i]
        last_line = line
        if kind == 'nl':
            at_start = True
            i += 1
            continue
        if kind == 'pp' and at_start:
            j = i + 1
            body = []
            while j < len(toks) and toks[j][0] != 'nl':
                body.append(toks[j])
                j += 1
            if not body or body[0][1] != 'define':
                name = body[0][1] if body else ''
                raise CompileError(f"unsupported preprocessor directive '#{name}'", line)
            if len(body) < 2 or body[1][0] != 'id':
                raise CompileError("macro name expected after #define", line)
            name = body[1]
            if len(body) > 2 and body[2][1] == '(' and body[2][3] == name[3] + len(name[1]):
                raise CompileError("macros with arguments are not supported", line)
            macros[name[1]] = body[2:]
            i = j
            continue
        at_start = False
        out.extend(expand(toks[i], frozenset()))
        i += 1
    out.append(('eof', '', last_line, 0))
    return out


def unescape(body, line):
    out = []
    i = 0
    while i < len(body):
        c = body[i]
        if c == '\\':
            i += 1
            d = body[i]
            if d not in ESCAPES:
                raise CompileError(f"unknown escape sequence '\\{d}'", line)
            out.append(ESCAPES[d])
        else:
            if ord(c) > M:
                raise CompileError("character out of range", line)
            out.append(ord(c))
        i += 1
    return out


class Ty:
    __slots__ = ('k', 'base', 'n', 'const')

    def __init__(self, k, base=None, n=None, const=False):
        self.k = k
        self.base = base
        self.n = n
        self.const = const

    def with_const(self, c):
        return Ty(self.k, self.base, self.n, c)

    def isint(self):
        return self.k in ('int', 'uint')

    def isptr(self):
        return self.k == 'ptr'

    def isscalar(self):
        return self.k in ('int', 'uint', 'ptr')

    def __str__(self):
        c = 'const ' if self.const else ''
        if self.k == 'int':
            return c + 'int'
        if self.k == 'uint':
            return c + 'unsigned'
        if self.k == 'void':
            return 'void'
        if self.k == 'ptr':
            return f'{self.base}*' + (' const' if self.const else '')
        return f'{self.base}[{self.n}]'


INT = Ty('int')
UINT = Ty('uint')
VOID = Ty('void')


def same_type(a, b):
    if a.k != b.k:
        return False
    if a.k == 'ptr':
        return same_type(a.base, b.base)
    if a.k == 'arr':
        return a.n == b.n and same_type(a.base, b.base)
    return True


INTRINSICS = {
    'in0': (UINT, [], 'IN0'), 'in1': (UINT, [], 'IN1'), 'in2': (UINT, [], 'IN2'),
    'out0': (VOID, [UINT], 'OUT0'), 'out1': (VOID, [UINT], 'OUT1'), 'out2': (VOID, [UINT], 'OUT2'),
    'rand': (UINT, [], 'RAND'),
    'sin': (INT, [INT], 'SIN'), 'cos': (INT, [INT], 'COS'), 'tan': (INT, [INT], 'TAN'),
    'sqrt': (UINT, [UINT], 'SQRT'),
    'pow': (UINT, [UINT, UINT], 'POW'),
    'abs': (INT, [INT], 'ABS'),
    'min': (UINT, [UINT, UINT], 'MIN'), 'max': (UINT, [UINT, UINT], 'MAX'),
    'halt': (VOID, [], 'HLT'),
}


class N:
    def __init__(self, kind, line, **kw):
        self.kind = kind
        self.line = line
        self.ty = None
        self.cv = None
        self.__dict__.update(kw)

    def children(self):
        out = []
        for name in ('a', 'b', 'c'):
            v = self.__dict__.get(name)
            if isinstance(v, N):
                out.append(v)
        out.extend(self.__dict__.get('args', []))
        return out


def has_side_effects(e):
    if e.kind in ('assign', 'incdec', 'call'):
        return True
    return any(has_side_effects(c) for c in e.children())


class Sym:
    def __init__(self, name, kind, ty, line):
        self.name = name
        self.kind = kind
        self.ty = ty
        self.line = line
        self.addr = None
        self.used = False
        self.ret = None
        self.params = []
        self.defined = False
        self.calls = set()
        self.body = None
        self.label = None


def fold_bin(op, a, b, unsigned):
    sa, sb = s16(a), s16(b)
    if op == '+':
        return (a + b) & M
    if op == '-':
        return (a - b) & M
    if op == '*':
        return (a * b) & M
    if op == '&':
        return a & b
    if op == '|':
        return a | b
    if op == '^':
        return a ^ b
    if op == '<<':
        return (a << b) & M if b < 16 else 0
    if op == '>>':
        if unsigned:
            return a >> b if b < 16 else 0
        if b >= 16:
            return M if a & 0x8000 else 0
        return (sa >> b) & M
    if op in ('/', '%'):
        if b == 0:
            return 0
        if unsigned:
            return (a // b if op == '/' else a % b) & M
        q = abs(sa) // abs(sb)
        r = abs(sa) % abs(sb)
        if op == '/':
            return (-q if (sa < 0) != (sb < 0) else q) & M
        return (-r if sa < 0 else r) & M
    x, y = (a, b) if unsigned else (sa, sb)
    if op == '==':
        return int(x == y)
    if op == '!=':
        return int(x != y)
    if op == '<':
        return int(x < y)
    if op == '<=':
        return int(x <= y)
    if op == '>':
        return int(x > y)
    if op == '>=':
        return int(x >= y)
    raise AssertionError(op)


class Parser:
    def __init__(self, toks, offsets=None):
        self.toks = toks
        self.offsets = offsets or {}
        self.i = 0
        self.scopes = [{}]
        self.funcs = []
        self.lower_names = {}
        self.data = []
        self.strings = {}
        self.frame_low = FRAME_TOP
        self.cur = None
        self.loops = 0
        self.warnings = []

    def peek(self, k=0):
        return self.toks[min(self.i + k, len(self.toks) - 1)]

    def next(self):
        t = self.toks[self.i]
        if t[0] != 'eof':
            self.i += 1
        return t

    def at(self, val, k=0):
        t = self.peek(k)
        return t[1] == val and t[0] in ('p', 'id')

    def accept(self, val):
        if self.at(val):
            return self.next()
        return None

    def expect(self, val):
        if not self.at(val):
            t = self.peek()
            found = 'end of file' if t[0] == 'eof' else f"'{t[1]}'"
            raise CompileError(f"expected '{val}' but found {found}", t[2])
        return self.next()

    def err(self, msg, line=None):
        raise CompileError(msg, self.peek()[2] if line is None else line)

    def warn(self, msg, line):
        self.warnings.append((line, msg))

    def ident(self):
        t = self.next()
        if t[0] != 'id':
            raise CompileError(f"identifier expected but found '{t[1]}'", t[2])
        if t[1] in UNSUPPORTED:
            raise CompileError(f"'{t[1]}' is not supported", t[2])
        if t[1] in TYPE_KW or t[1] in STMT_KW:
            raise CompileError(f"unexpected keyword '{t[1]}'", t[2])
        return t

    def lookup(self, name):
        for scope in reversed(self.scopes):
            if name in scope:
                return scope[name]
        return None

    def declare(self, sym):
        if sym.name in INTRINSICS:
            raise CompileError(f"'{sym.name}' is a reserved intrinsic name", sym.line)
        if sym.name in self.scopes[-1]:
            raise CompileError(f"redeclaration of '{sym.name}'", sym.line)
        self.scopes[-1][sym.name] = sym

    def alloc_data(self, words):
        addr = DATA_BASE + len(self.data)
        self.data.extend(words)
        return addr

    def alloc_frame(self, n):
        self.frame_low -= n
        return self.frame_low

    def intern(self, values):
        key = tuple(values)
        if key not in self.strings:
            self.strings[key] = self.alloc_data(list(values) + [0])
        return self.strings[key]

    def parse_program(self):
        while self.peek()[0] != 'eof':
            self.top_level()
        main = self.lookup('main')
        if main is None or main.kind != 'func' or not main.defined:
            raise CompileError("function 'main' is not defined", 0)
        if main.params:
            raise CompileError("'main' must take no parameters", main.line)
        for f in self.funcs:
            for c in f.calls:
                g = self.lookup(c)
                if not g.defined:
                    raise CompileError(f"function '{c}' is declared but never defined", f.line)
        self.check_recursion()
        return main

    def frame_offsets(self, main):
        order = []
        seen = set()

        def dfs(f):
            seen.add(f.name)
            for c in sorted(f.calls):
                if c not in seen:
                    dfs(self.lookup(c))
            order.append(f)

        dfs(main)
        offsets = {main.name: 0}
        total = 0
        for f in reversed(order):
            end = offsets.get(f.name, 0) + f.fsize
            total = max(total, end)
            for c in f.calls:
                offsets[c] = max(offsets.get(c, 0), end)
        return offsets, total

    def check_recursion(self):
        state = {}

        def dfs(f, path):
            state[f.name] = 1
            for c in sorted(f.calls):
                g = self.lookup(c)
                if state.get(c) == 1:
                    cycle = path[path.index(g):] + [g]
                    raise CompileError("recursion " + " -> ".join(x.name for x in cycle), g.line)
                if c not in state:
                    dfs(g, path + [g])
            state[f.name] = 2

        for f in self.funcs:
            if f.name not in state:
                dfs(f, [f])

    def parse_spec(self):
        const = False
        base = None
        while True:
            t = self.peek()
            v = t[1]
            if t[0] != 'id':
                break
            if v == 'const':
                const = True
                self.next()
            elif v == 'int' and base in (None, 'unsigned'):
                self.next()
                base = base or 'int'
            elif v == 'unsigned' and base is None:
                self.next()
                base = 'unsigned'
            elif v == 'void' and base is None:
                self.next()
                base = 'void'
            elif v in UNSUPPORTED:
                raise CompileError(f"'{v}' is not supported", t[2])
            else:
                break
        if base is None:
            self.err("type expected")
        ty = {'int': INT, 'unsigned': UINT, 'void': VOID}[base]
        return ty.with_const(const) if const else ty

    def is_type_start(self, k=0):
        t = self.peek(k)
        return t[0] == 'id' and t[1] in TYPE_START

    def pointer_stars(self, ty, line):
        while self.accept('*'):
            if ty.k == 'void':
                raise CompileError("pointers to void are not supported", line)
            const = bool(self.accept('const'))
            ty = Ty('ptr', ty, const=const)
        return ty

    def const_expr(self, what):
        e = self.rv(self.assign())
        if e.cv is None:
            raise CompileError(f"{what} is not a constant expression", e.line)
        return e

    def top_level(self):
        spec = self.parse_spec()
        line = self.peek()[2]
        ty = self.pointer_stars(spec, line)
        name = self.ident()
        if self.at('('):
            self.function(ty, name)
            return
        while True:
            self.global_var(spec, ty, name)
            if self.accept(','):
                ty = self.pointer_stars(spec, line)
                name = self.ident()
                continue
            self.expect(';')
            return

    def array_suffix(self, ty, name, allow_empty):
        if not self.accept('['):
            return ty
        line = name[2]
        if ty.k == 'void':
            raise CompileError("array of void", line)
        n = None
        if not self.at(']'):
            e = self.const_expr("array size")
            n = e.cv
            if n == 0 or n > 0x8000:
                raise CompileError("invalid array size", line)
        elif not allow_empty:
            raise CompileError("array size required", line)
        self.expect(']')
        if self.at('['):
            raise CompileError("multi-dimensional arrays are not supported", line)
        return Ty('arr', ty, n)

    def global_var(self, spec, ty, nametok):
        name, line = nametok[1], nametok[2]
        if spec.k == 'void' and ty.k == 'void':
            raise CompileError("variable declared void", line)
        ty = self.array_suffix(ty, nametok, True)
        words = None
        if self.accept('='):
            if ty.k == 'arr':
                words = self.array_init(ty, line)
                if ty.n is None:
                    ty = Ty('arr', ty.base, len(words))
                elif len(words) > ty.n:
                    raise CompileError("too many initializers", line)
                words = words + [0] * (ty.n - len(words))
            else:
                e = self.rv(self.assign())
                self.conv(e, ty, line, "initializer")
                if e.cv is None:
                    raise CompileError("initializer is not a constant expression", line)
                words = [e.cv]
        elif ty.k == 'arr' and ty.n is None:
            raise CompileError("array size required", line)
        size = ty.n if ty.k == 'arr' else 1
        sym = Sym(name, 'global', ty, line)
        sym.addr = self.alloc_data(words if words is not None else [0] * size)
        self.declare(sym)

    def array_init(self, ty, line):
        t = self.peek()
        if t[0] == 'str':
            self.next()
            return unescape(t[1][1:-1], t[2]) + [0]
        self.expect('{')
        words = []
        while not self.at('}'):
            e = self.rv(self.assign())
            self.conv(e, ty.base, line, "initializer")
            if e.cv is None:
                raise CompileError("initializer is not a constant expression", e.line)
            words.append(e.cv)
            if not self.accept(','):
                break
        self.expect('}')
        if not words:
            raise CompileError("empty initializer", line)
        return words

    def function(self, ret, nametok):
        name, line = nametok[1], nametok[2]
        if name in INTRINSICS:
            raise CompileError(f"'{name}' is a reserved intrinsic name", line)
        self.expect('(')
        params = []
        if self.at('void') and self.at(')', 1):
            self.next()
        elif not self.at(')'):
            while True:
                spec = self.parse_spec()
                if spec.k == 'void':
                    raise CompileError("parameter declared void", self.peek()[2])
                pty = self.pointer_stars(spec, line)
                pname = self.ident()
                if self.accept('['):
                    if not self.at(']'):
                        self.const_expr("array size")
                    self.expect(']')
                    pty = Ty('ptr', pty)
                params.append((pty, pname))
                if not self.accept(','):
                    break
        self.expect(')')
        if len(params) > 4:
            raise CompileError(f"function '{name}' has more than 4 parameters", line)
        if ret.k == 'ptr':
            raise CompileError("functions returning pointers are not supported", line)
        existing = self.scopes[0].get(name)
        if existing is not None:
            if existing.kind != 'func':
                raise CompileError(f"redeclaration of '{name}' as a function", line)
            sig_ok = (same_type(existing.ret, ret) and len(existing.sig) == len(params)
                      and all(same_type(a, b[0]) for a, b in zip(existing.sig, params)))
            if not sig_ok:
                raise CompileError(f"conflicting types for '{name}'", line)
            sym = existing
        else:
            low = name.lower()
            if low in self.lower_names:
                raise CompileError(f"'{name}' and '{self.lower_names[low]}' differ only by case", line)
            self.lower_names[low] = name
            sym = Sym(name, 'func', None, line)
            sym.ret = ret
            sym.sig = [p[0] for p in params]
            sym.label = 'F_' + name
            self.scopes[0][name] = sym
        if self.accept(';'):
            return
        if sym.defined:
            raise CompileError(f"redefinition of '{name}'", line)
        sym.defined = True
        sym.line = line
        self.funcs.append(sym)
        self.cur = sym
        self.frame_low = FRAME_TOP - self.offsets.get(name, 0)
        frame_top = self.frame_low
        self.scopes.append({})
        sym.params = []
        for pty, pname in params:
            p = Sym(pname[1], 'local', pty, pname[2])
            p.addr = self.alloc_frame(1)
            self.declare(p)
            sym.params.append(p)
        sym.body = self.block_body()
        sym.fsize = frame_top - self.frame_low
        self.scopes.pop()
        self.cur = None

    def block_body(self):
        line = self.expect('{')[2]
        items = []
        while not self.at('}'):
            if self.peek()[0] == 'eof':
                raise CompileError("unexpected end of file, '}' missing", self.peek()[2])
            items.append(self.statement())
        self.expect('}')
        return N('block', line, items=items)

    def block(self):
        self.scopes.append({})
        b = self.block_body()
        self.scopes.pop()
        return b

    def statement(self):
        t = self.peek()
        kind, v, line = t[0], t[1], t[2]
        if kind == 'p':
            if v == '{':
                return self.block()
            if v == ';':
                self.next()
                return N('empty', line)
        if kind == 'id':
            if self.is_type_start():
                return self.local_decl()
            if v == 'if':
                self.next()
                self.expect('(')
                c = self.cond_expr()
                self.expect(')')
                a = self.statement()
                b = self.statement() if self.accept('else') else None
                return N('if', line, c=c, a=a, b=b)
            if v == 'while':
                self.next()
                self.expect('(')
                c = self.cond_expr()
                self.expect(')')
                self.loops += 1
                body = self.statement()
                self.loops -= 1
                return N('while', line, c=c, a=body)
            if v == 'do':
                self.next()
                self.loops += 1
                body = self.statement()
                self.loops -= 1
                self.expect('while')
                self.expect('(')
                c = self.cond_expr()
                self.expect(')')
                self.expect(';')
                return N('dowhile', line, c=c, a=body)
            if v == 'for':
                return self.for_stmt()
            if v == 'return':
                self.next()
                e = None
                if not self.at(';'):
                    e = self.rv(self.expr())
                    if self.cur.ret.k == 'void':
                        raise CompileError("void function should not return a value", line)
                    self.conv(e, self.cur.ret, line, "return")
                elif self.cur.ret.k != 'void':
                    raise CompileError("non-void function should return a value", line)
                self.expect(';')
                return N('return', line, a=e)
            if v in ('break', 'continue'):
                self.next()
                if not self.loops:
                    raise CompileError(f"'{v}' outside of a loop", line)
                self.expect(';')
                return N(v, line)
            if v == 'else':
                raise CompileError("'else' without matching 'if'", line)
            if v in UNSUPPORTED:
                raise CompileError(f"'{v}' is not supported", line)
        e = self.expr()
        self.expect(';')
        return N('expr', line, a=e)

    def for_stmt(self):
        line = self.next()[2]
        self.expect('(')
        self.scopes.append({})
        init = None
        if self.is_type_start():
            init = self.local_decl()
        elif not self.accept(';'):
            init = N('expr', line, a=self.expr())
            self.expect(';')
        c = None
        if not self.at(';'):
            c = self.cond_expr()
        self.expect(';')
        step = None
        if not self.at(')'):
            step = N('expr', line, a=self.expr())
        self.expect(')')
        self.loops += 1
        body = self.statement()
        self.loops -= 1
        self.scopes.pop()
        return N('for', line, init=init, c=c, step=step, a=body)

    def cond_expr(self):
        e = self.rv(self.expr())
        if not e.ty.isscalar():
            raise CompileError("condition must be a scalar value", e.line)
        return e

    def local_decl(self):
        spec = self.parse_spec()
        if spec.k == 'void':
            raise CompileError("variable declared void", self.peek()[2])
        items = []
        while True:
            line = self.peek()[2]
            ty = self.pointer_stars(spec, line)
            nametok = self.ident()
            ty = self.array_suffix(ty, nametok, False)
            sym = Sym(nametok[1], 'local', ty, nametok[2])
            sym.addr = self.alloc_frame(ty.n if ty.k == 'arr' else 1)
            self.declare(sym)
            if self.accept('='):
                if ty.k == 'arr':
                    raise CompileError("initializers for local arrays are not supported", nametok[2])
                var = N('var', nametok[2], sym=sym, ty=ty)
                rhs = self.rv(self.assign())
                self.conv(rhs, ty, nametok[2], "initializer")
                items.append(N('expr', nametok[2], a=N('assign', nametok[2], a=var, b=rhs, ty=ty.with_const(False))))
            if not self.accept(','):
                break
        self.expect(';')
        return N('seq', items[0].line if items else 0, items=items)

    def expr(self):
        e = self.assign()
        if self.at(','):
            raise CompileError("the comma operator is not supported", self.peek()[2])
        return e

    def assign(self):
        lhs = self.ternary()
        t = self.peek()
        if t[0] == 'p' and t[1] in ASSIGN_OPS:
            self.next()
            rhs = self.assign()
            return self.make_assign(t[1], lhs, rhs, t[2])
        return lhs

    def ternary(self):
        c = self.binary(1)
        if self.at('?'):
            line = self.next()[2]
            a = self.expr()
            self.expect(':')
            b = self.ternary()
            return self.make_cond(c, a, b, line)
        return c

    def binary(self, minprec):
        lhs = self.unary()
        while True:
            t = self.peek()
            prec = BINPREC.get(t[1]) if t[0] == 'p' else None
            if prec is None or prec < minprec:
                return lhs
            self.next()
            rhs = self.binary(prec + 1)
            lhs = self.make_binary(t[1], lhs, rhs, t[2])

    def unary(self):
        t = self.peek()
        if t[0] == 'p':
            v, line = t[1], t[2]
            if v in ('-', '+', '!', '~'):
                self.next()
                return self.make_unary(v, self.unary(), line)
            if v == '*':
                self.next()
                return self.make_deref(self.unary(), line)
            if v == '&':
                self.next()
                return self.make_addr(self.unary(), line)
            if v in ('++', '--'):
                self.next()
                return self.make_incdec(v, True, self.unary(), line)
            if v == '(' and self.is_type_start(1):
                self.next()
                spec = self.parse_spec()
                ty = self.pointer_stars(spec, line)
                self.expect(')')
                return self.make_cast(ty, self.unary(), line)
        return self.postfix()

    def postfix(self):
        e = self.primary()
        while True:
            t = self.peek()
            if t[0] != 'p':
                return e
            if t[1] == '[':
                self.next()
                idx = self.expr()
                self.expect(']')
                e = self.make_index(e, idx, t[2])
            elif t[1] in ('++', '--'):
                self.next()
                e = self.make_incdec(t[1], False, e, t[2])
            else:
                return e

    def primary(self):
        t = self.next()
        kind, v, line = t[0], t[1], t[2]
        if kind == 'num':
            try:
                n = int(v, 0)
            except ValueError:
                raise CompileError("octal constants are not supported", line)
            if n > M:
                raise CompileError("integer constant is too large", line)
            return N('num', line, ty=INT if n <= 0x7FFF else UINT, cv=n)
        if kind == 'chr':
            return N('num', line, ty=INT, cv=unescape(v[1:-1], line)[0])
        if kind == 'str':
            values = unescape(v[1:-1], line)
            return N('num', line, ty=Ty('ptr', INT), cv=self.intern(values))
        if kind == 'p' and v == '(':
            e = self.expr()
            self.expect(')')
            return e
        if kind == 'id':
            if v in UNSUPPORTED:
                raise CompileError(f"'{v}' is not supported", line)
            if v in TYPE_KW or v in STMT_KW:
                raise CompileError(f"unexpected keyword '{v}'", line)
            if self.at('('):
                return self.call(v, line)
            sym = self.lookup(v)
            if sym is None:
                raise CompileError(f"'{v}' is undeclared", line)
            if sym.kind == 'func':
                raise CompileError(f"function '{v}' used as a value", line)
            sym.used = True
            return N('var', line, sym=sym, ty=sym.ty)
        found = 'end of file' if kind == 'eof' else f"'{v}'"
        raise CompileError(f"expression expected but found {found}", line)

    def rv(self, e):
        if e.ty.k == 'arr':
            return N('num', e.line, ty=Ty('ptr', e.ty.base), cv=e.sym.addr)
        return e

    def need_value(self, e):
        if e.ty.k == 'void':
            raise CompileError("void value used in an expression", e.line)

    def conv(self, e, t, line, what):
        s = e.ty
        if s.k == 'void':
            raise CompileError(f"{what}: void value used in an expression", line)
        if t.isint():
            if s.isptr():
                raise CompileError(f"{what}: cannot convert a pointer to an integer without a cast", line)
            return
        if t.isptr():
            if s.isint():
                if e.cv == 0:
                    return
                raise CompileError(f"{what}: cannot convert an integer to a pointer without a cast", line)
            if s.base.k != t.base.k:
                self.warn(f"{what}: incompatible pointer types ({s} to {t})", line)
            elif s.base.const and not t.base.const:
                self.warn(f"{what}: discards the const qualifier", line)
            return
        raise CompileError(f"{what}: invalid conversion", line)

    def call(self, name, line):
        self.expect('(')
        args = []
        if not self.at(')'):
            while True:
                args.append(self.rv(self.assign()))
                if not self.accept(','):
                    break
        self.expect(')')
        intr = None
        if name in INTRINSICS:
            ret, ptys, intr = INTRINSICS[name]
            sym = None
        else:
            sym = self.lookup(name)
            if sym is None:
                raise CompileError(f"'{name}' is undeclared", line)
            if sym.kind != 'func':
                raise CompileError(f"'{name}' is not a function", line)
            ret, ptys = sym.ret, sym.sig
            if self.cur is not None:
                self.cur.calls.add(name)
        if len(args) != len(ptys):
            raise CompileError(f"'{name}' expects {len(ptys)} argument(s), got {len(args)}", line)
        for i, (a, p) in enumerate(zip(args, ptys)):
            self.conv(a, p, line, f"argument {i + 1} of '{name}'")
        return N('call', line, name=name, sym=sym, intr=intr, args=args, ty=ret)

    def make_unary(self, op, a, line):
        a = self.rv(a)
        self.need_value(a)
        if op == '!':
            if not a.ty.isscalar():
                raise CompileError("invalid operand to '!'", line)
            cv = None if a.cv is None else int(a.cv == 0)
            return N('not', line, a=a, ty=INT, cv=cv)
        if not a.ty.isint():
            raise CompileError(f"invalid operand to unary '{op}'", line)
        if op == '+':
            return a
        cv = None
        if a.cv is not None:
            cv = (-a.cv) & M if op == '-' else (~a.cv) & M
        return N('neg' if op == '-' else 'bnot', line, a=a, ty=a.ty.with_const(False), cv=cv)

    def make_binary(self, op, l, r, line):
        l = self.rv(l)
        r = self.rv(r)
        self.need_value(l)
        self.need_value(r)
        if op in ('&&', '||'):
            if not l.ty.isscalar() or not r.ty.isscalar():
                raise CompileError(f"invalid operands to '{op}'", line)
            cv = None
            if l.cv is not None:
                if op == '&&':
                    cv = 0 if l.cv == 0 else (None if r.cv is None else int(r.cv != 0))
                else:
                    cv = 1 if l.cv != 0 else (None if r.cv is None else int(r.cv != 0))
            return N('land' if op == '&&' else 'lor', line, a=l, b=r, ty=INT, cv=cv)
        if op in CMP_OPS:
            if l.ty.isptr() != r.ty.isptr():
                other = r if l.ty.isptr() else l
                if other.cv is None:
                    raise CompileError("comparison between a pointer and an integer", line)
            unsigned = l.ty.k != 'int' or r.ty.k != 'int'
            cv = None
            if l.cv is not None and r.cv is not None:
                cv = fold_bin(op, l.cv, r.cv, unsigned)
            return N('cmp', line, op=op, a=l, b=r, unsigned=unsigned, ty=INT, cv=cv)
        lp, rp = l.ty.isptr(), r.ty.isptr()
        if op == '+':
            if lp and rp:
                raise CompileError("cannot add two pointers", line)
            ty = l.ty if lp else r.ty if rp else None
        elif op == '-':
            if rp and not lp:
                raise CompileError("cannot subtract a pointer from an integer", line)
            ty = INT if lp and rp else l.ty if lp else None
        else:
            if lp or rp:
                raise CompileError(f"invalid operands to '{op}' (pointer)", line)
            ty = None
        if ty is None:
            if op in ('<<', '>>'):
                ty = l.ty
            else:
                ty = UINT if l.ty.k == 'uint' or r.ty.k == 'uint' else INT
        ty = ty.with_const(False)
        unsigned = l.ty.k != 'int' if op in ('<<', '>>') else (l.ty.k != 'int' or r.ty.k != 'int')
        if op in ('/', '%') and r.cv == 0:
            self.warn("division by zero (the result is 0 on this hardware)", line)
        cv = None
        if l.cv is not None and r.cv is not None:
            cv = fold_bin(op, l.cv, r.cv, unsigned)
        return N('binary', line, op=op, a=l, b=r, unsigned=unsigned, ty=ty, cv=cv)

    def make_cond(self, c, a, b, line):
        c = self.rv(c)
        a = self.rv(a)
        b = self.rv(b)
        for x in (c, a, b):
            self.need_value(x)
        if not c.ty.isscalar():
            raise CompileError("condition must be a scalar value", line)
        if a.ty.isint() and b.ty.isint():
            ty = UINT if a.ty.k == 'uint' or b.ty.k == 'uint' else INT
        elif a.ty.isptr() and b.ty.isptr():
            ty = a.ty
        elif a.ty.isptr() and b.cv == 0:
            ty = a.ty
        elif b.ty.isptr() and a.cv == 0:
            ty = b.ty
        else:
            raise CompileError("incompatible operand types in '?:'", line)
        cv = None
        if c.cv is not None:
            cv = a.cv if c.cv else b.cv
        return N('cond', line, c=c, a=a, b=b, ty=ty.with_const(False), cv=cv)

    def is_lvalue(self, e):
        return e.kind in ('var', 'index', 'deref')

    def make_assign(self, op, lv, rhs, line):
        if not self.is_lvalue(lv) or lv.ty.k == 'arr':
            raise CompileError("the left side of the assignment is not assignable", line)
        if lv.ty.const:
            raise CompileError("assignment to a read-only object", line)
        if op != '=':
            if has_side_effects(lv):
                raise CompileError("compound assignment on an operand with side effects is not supported", line)
            rhs = self.make_binary(op[:-1], lv, rhs, line)
        rhs = self.rv(rhs)
        self.conv(rhs, lv.ty, line, "assignment")
        return N('assign', line, a=lv, b=rhs, ty=lv.ty.with_const(False))

    def make_incdec(self, op, prefix, lv, line):
        if not self.is_lvalue(lv) or lv.ty.k == 'arr' or not lv.ty.isscalar():
            raise CompileError(f"operand of '{op}' is not a modifiable scalar", line)
        if lv.ty.const:
            raise CompileError("modification of a read-only object", line)
        return N('incdec', line, op=op, prefix=prefix, a=lv, ty=lv.ty.with_const(False))

    def make_deref(self, p, line):
        p = self.rv(p)
        if not p.ty.isptr():
            raise CompileError("dereference of a non-pointer", line)
        return N('deref', line, a=p, ty=p.ty.base)

    def make_index(self, base, idx, line):
        base = self.rv(base)
        idx = self.rv(idx)
        if not base.ty.isptr():
            raise CompileError("subscripted value is not an array or pointer", line)
        if not idx.ty.isint():
            raise CompileError("array subscript is not an integer", line)
        return N('index', line, a=base, b=idx, ty=base.ty.base)

    def make_addr(self, e, line):
        if not self.is_lvalue(e):
            raise CompileError("cannot take the address of this expression", line)
        if e.kind == 'var' and e.ty.k == 'arr':
            return N('num', line, ty=Ty('ptr', e.ty.base), cv=e.sym.addr)
        node = N('addr', line, a=e, ty=Ty('ptr', e.ty))
        node.cv = abs_addr(e)
        return node

    def make_cast(self, ty, e, line):
        e = self.rv(e)
        if ty.k == 'void':
            return N('cast', line, a=e, ty=VOID)
        self.need_value(e)
        return N('cast', line, a=e, ty=ty, cv=e.cv)


def abs_addr(lv):
    if lv.kind == 'var':
        return lv.sym.addr
    if lv.kind == 'index':
        if lv.a.cv is not None and lv.b.cv is not None:
            return (lv.a.cv + lv.b.cv) & M
        return None
    if lv.kind == 'deref':
        return lv.a.cv
    return None


class Gen:
    def __init__(self, parser):
        self.p = parser
        self.out = []
        self.nlabel = 0
        self.busy = set()
        self.runtime = set()
        self.loop_labels = []
        self.cur = None

    def emit(self, s):
        self.out.append('    ' + s)

    def label(self, name):
        self.out.append(name + ':')

    def newlabel(self):
        self.nlabel += 1
        return f'L{self.nlabel}'

    def alloc(self):
        for r in range(POOL):
            if r not in self.busy:
                self.busy.add(r)
                return r
        raise AssertionError("register pool exhausted")

    def free(self, r):
        self.busy.discard(r)

    def nfree(self):
        return POOL - len(self.busy)

    def pair(self, fa, fb):
        ra = fa()
        spilled = False
        if self.nfree() < 2:
            self.emit(f'PUSH R{ra}')
            self.free(ra)
            spilled = True
        rb = fb()
        if spilled:
            ra = self.alloc()
            self.emit(f'POP R{ra}')
        return ra, rb

    def compile(self, main):
        reachable = []
        seen = set()
        stack = [main]
        while stack:
            f = stack.pop()
            if f.name in seen:
                continue
            seen.add(f.name)
            reachable.append(f)
            for c in f.calls:
                stack.append(self.p.lookup(c))
        order = [f for f in self.p.funcs if f.name in seen]
        self.emit('JSR F_main')
        self.emit('HLT')
        for f in order:
            self.function(f)
        self.emit_runtime()
        return self.peephole(self.out)

    def function(self, f):
        self.cur = f
        self.label(f.label)
        for i, p in enumerate(f.params):
            if p.used:
                self.emit(f'STORE R{i}, {p.addr}')
        self.stmt(f.body)
        self.emit('RTS')

    def peephole(self, lines):
        out = []
        for i, ln in enumerate(lines):
            if ln.startswith('    JMP ') and i + 1 < len(lines) and lines[i + 1] == ln.split()[1] + ':':
                continue
            out.append(ln)
        return out

    def stmt(self, s):
        k = s.kind
        if k == 'block' or k == 'seq':
            for it in s.items:
                self.stmt(it)
        elif k == 'empty':
            pass
        elif k == 'expr':
            r = self.gen(s.a, False)
            if r is not None:
                self.free(r)
        elif k == 'if':
            lelse = self.newlabel()
            self.jump_if(s.c, lelse, False)
            self.stmt(s.a)
            if s.b is not None:
                lend = self.newlabel()
                self.emit(f'JMP {lend}')
                self.label(lelse)
                self.stmt(s.b)
                self.label(lend)
            else:
                self.label(lelse)
        elif k == 'while':
            ltop, lend = self.newlabel(), self.newlabel()
            self.label(ltop)
            self.jump_if(s.c, lend, False)
            self.loop_labels.append((lend, ltop))
            self.stmt(s.a)
            self.loop_labels.pop()
            self.emit(f'JMP {ltop}')
            self.label(lend)
        elif k == 'dowhile':
            ltop, lcont, lend = self.newlabel(), self.newlabel(), self.newlabel()
            self.label(ltop)
            self.loop_labels.append((lend, lcont))
            self.stmt(s.a)
            self.loop_labels.pop()
            self.label(lcont)
            self.jump_if(s.c, ltop, True)
            self.label(lend)
        elif k == 'for':
            ltop, lcont, lend = self.newlabel(), self.newlabel(), self.newlabel()
            if s.init is not None:
                self.stmt(s.init)
            self.label(ltop)
            if s.c is not None:
                self.jump_if(s.c, lend, False)
            self.loop_labels.append((lend, lcont))
            self.stmt(s.a)
            self.loop_labels.pop()
            self.label(lcont)
            if s.step is not None:
                self.stmt(s.step)
            self.emit(f'JMP {ltop}')
            self.label(lend)
        elif k == 'return':
            if s.a is not None:
                r = self.gen(s.a)
                if r != 0:
                    self.emit(f'MOV R0, R{r}')
                self.free(r)
            self.emit('RTS')
        elif k == 'break':
            self.emit(f'JMP {self.loop_labels[-1][0]}')
        elif k == 'continue':
            self.emit(f'JMP {self.loop_labels[-1][1]}')
        else:
            raise AssertionError(k)
        assert not self.busy, (s.kind, s.line, self.busy)

    def jump_if(self, e, label, sense):
        if e.cv is not None:
            if bool(e.cv) == sense:
                self.emit(f'JMP {label}')
            return
        k = e.kind
        if k == 'not':
            self.jump_if(e.a, label, not sense)
        elif k == 'land':
            if sense:
                skip = self.newlabel()
                self.jump_if(e.a, skip, False)
                self.jump_if(e.b, label, True)
                self.label(skip)
            else:
                self.jump_if(e.a, label, False)
                self.jump_if(e.b, label, False)
        elif k == 'lor':
            if sense:
                self.jump_if(e.a, label, True)
                self.jump_if(e.b, label, True)
            else:
                skip = self.newlabel()
                self.jump_if(e.a, skip, True)
                self.jump_if(e.b, label, False)
                self.label(skip)
        elif k == 'cmp':
            self.compare_jump(e, label, sense)
        else:
            r = self.gen(e)
            self.emit(f'COMPI R{r}, 0')
            self.free(r)
            self.emit(f'{"JNE" if sense else "JE"} {label}')

    def compare_jump(self, e, label, sense):
        op, l, r = e.op, e.a, e.b
        if l.cv is not None and r.cv is None:
            l, r = r, l
            op = MIRROR[op]
        if r.cv is not None:
            rl = self.gen(l)
            self.emit(f'COMPI R{rl}, {r.cv}')
        elif r.kind == 'var' and r.ty.isscalar():
            rl = self.gen(l)
            self.emit(f'COMPM R{rl}, {r.sym.addr}')
        else:
            rl, rr = self.pair(lambda: self.gen(l), lambda: self.gen(r))
            self.emit(f'CMP R{rl}, R{rr}')
            self.free(rr)
        self.free(rl)
        if not sense:
            op = INVERT[op]
        table = JCC_UNSIGNED if e.unsigned else JCC_SIGNED
        self.emit(f'{table[op]} {label}')

    def gen(self, e, want=True):
        if e.cv is not None:
            if not want:
                return None
            r = self.alloc()
            self.emit(f'MOVI R{r}, {e.cv}')
            return r
        return getattr(self, 'g_' + e.kind)(e, want)

    def is_simple(self, x):
        return x.cv is not None or (x.kind == 'var' and x.ty.isscalar())

    def load_simple(self, x, reg):
        if x.cv is not None:
            self.emit(f'MOVI R{reg}, {x.cv}')
        else:
            self.emit(f'LOAD R{reg}, {x.sym.addr}')

    def g_var(self, e, want):
        r = self.alloc()
        self.emit(f'LOAD R{r}, {e.sym.addr}')
        return r

    def g_cast(self, e, want):
        if e.ty.k == 'void':
            r = self.gen(e.a, False)
            if r is not None:
                self.free(r)
            return None
        return self.gen(e.a, want)

    def g_neg(self, e, want):
        r = self.gen(e.a)
        self.emit(f'MOVI R{TMP}, 0')
        self.emit(f'SUB R{r}, R{TMP}, R{r}')
        return r

    def g_bnot(self, e, want):
        r = self.gen(e.a)
        self.emit(f'NOT R{r}, R{r}')
        return r

    def cond_value(self, e, want):
        lfalse, lend = self.newlabel(), self.newlabel()
        self.jump_if(e, lfalse, False)
        r = self.alloc()
        self.emit(f'MOVI R{r}, 1')
        self.emit(f'JMP {lend}')
        self.label(lfalse)
        self.emit(f'MOVI R{r}, 0')
        self.label(lend)
        return r

    g_not = g_cmp = g_land = g_lor = cond_value

    def g_cond(self, e, want):
        lelse, lend = self.newlabel(), self.newlabel()
        self.jump_if(e.c, lelse, False)
        snap = set(self.busy)
        ra = self.gen(e.a)
        after = set(self.busy)
        self.emit(f'JMP {lend}')
        self.label(lelse)
        self.busy = set(snap)
        rb = self.gen(e.b)
        if rb != ra:
            self.emit(f'MOV R{ra}, R{rb}')
        self.busy = after
        self.label(lend)
        return ra

    def g_binary(self, e, want):
        op, l, r = e.op, e.a, e.b
        if op in ('+', '-'):
            for x, y, flip in ((l, r, False), (r, l, True)):
                if flip and op == '-':
                    continue
                if y.cv in (1, M) and x.cv is None:
                    inc = (op == '+') == (y.cv == 1)
                    rx = self.gen(x)
                    self.emit(f'{"INC" if inc else "DEC"} R{rx}')
                    return rx
        if op in COMMUTATIVE and l.cv is not None and r.cv is None:
            l, r = r, l
        if self.is_simple(r):
            rl = self.gen(l)
            self.load_simple(r, TMP)
            rb = TMP
        else:
            rl, rb = self.pair(lambda: self.gen(l), lambda: self.gen(r))
        return self.arith(op, e.unsigned, rl, rb)

    def arith(self, op, unsigned, ra, rb):
        if op in ARITH_ALU:
            self.emit(f'{ARITH_ALU[op]} R{ra}, R{ra}, R{rb}')
        elif op == '/' and unsigned:
            self.emit(f'DIV R{ra}, R{ra}, R{rb}')
        elif op == '%' and unsigned:
            self.emit(f'MOD R{ra}, R{ra}, R{rb}')
        elif op == '>>' and unsigned:
            self.emit(f'SHR R{ra}, R{ra}, R{rb}')
        else:
            name = {'/': 'RT_SDIV', '%': 'RT_SMOD', '>>': 'RT_SAR'}[op]
            self.runtime.add(name)
            return self.call_runtime(name, [ra, rb])
        if rb < POOL:
            self.free(rb)
        return ra

    def call_runtime(self, name, regs):
        saved = [r for r in sorted(self.busy) if r not in regs]
        for r in saved:
            self.emit(f'PUSH R{r}')
        for r in regs:
            self.emit(f'PUSH R{r}')
        self.busy = set()
        for i in reversed(range(len(regs))):
            self.emit(f'POP R{i}')
        self.emit(f'JSR {name}')
        return self.finish_call(saved, True)

    def finish_call(self, saved, want):
        if not want:
            for r in reversed(saved):
                self.emit(f'POP R{r}')
                self.busy.add(r)
            return None
        if saved:
            self.emit(f'MOV R{TMP}, R0')
        for r in reversed(saved):
            self.emit(f'POP R{r}')
            self.busy.add(r)
        r = self.alloc()
        if saved:
            self.emit(f'MOV R{r}, R{TMP}')
        return r

    def g_call(self, e, want):
        if e.intr:
            return self.intrinsic(e, want)
        saved = sorted(self.busy)
        for r in saved:
            self.emit(f'PUSH R{r}')
        self.busy = set()
        n = len(e.args)
        late = []
        early = []
        for i, a in enumerate(e.args):
            simple = a.cv is not None or (a.kind == 'var' and a.ty.isscalar())
            if simple and not any(has_side_effects(b) for b in e.args[i + 1:]):
                late.append(i)
            else:
                early.append(i)
        for k, i in enumerate(early):
            r = self.gen(e.args[i])
            if k < len(early) - 1:
                self.emit(f'PUSH R{r}')
                self.free(r)
                continue
            targets = early[:-1]
            if r in targets:
                self.emit(f'PUSH R{r}')
                self.free(r)
                for j in reversed(early):
                    self.emit(f'POP R{j}')
            else:
                for j in reversed(targets):
                    self.emit(f'POP R{j}')
                if r != i:
                    self.emit(f'MOV R{i}, R{r}')
                self.free(r)
        for i in late:
            self.load_simple(e.args[i], i)
        self.emit(f'JSR {e.sym.label}')
        return self.finish_call(saved, want and e.ty.k != 'void')

    def intrinsic(self, e, want):
        ins = e.intr
        n = len(e.args)
        if ins == 'HLT':
            self.emit('HLT')
            return None
        if n == 0:
            r = self.alloc()
            self.emit(f'{ins} R{r}')
            return r
        if ins.startswith('OUT'):
            r = self.gen(e.args[0])
            self.emit(f'{ins} R{r}')
            self.free(r)
            return None
        if n == 1:
            r = self.gen(e.args[0])
            self.emit(f'{ins} R{r}, R{r}')
            return r
        ra, rb = self.pair(lambda: self.gen(e.args[0]), lambda: self.gen(e.args[1]))
        self.emit(f'{ins} R{ra}, R{ra}, R{rb}')
        self.free(rb)
        return ra

    def addr_reg(self, lv):
        a = abs_addr(lv)
        if a is not None:
            r = self.alloc()
            self.emit(f'MOVI R{r}, {a}')
            return r
        if lv.kind == 'deref':
            return self.gen(lv.a)
        base, idx = lv.a, lv.b
        if base.cv is not None:
            ri = self.gen(idx)
            if base.cv != 0:
                self.emit(f'MOVI R{TMP}, {base.cv}')
                self.emit(f'ADD R{ri}, R{ri}, R{TMP}')
            return ri
        if idx.cv is not None:
            rp = self.gen(base)
            if idx.cv == 1:
                self.emit(f'INC R{rp}')
            elif idx.cv == M:
                self.emit(f'DEC R{rp}')
            elif idx.cv != 0:
                self.emit(f'MOVI R{TMP}, {idx.cv}')
                self.emit(f'ADD R{rp}, R{rp}, R{TMP}')
            return rp
        rp, ri = self.pair(lambda: self.gen(base), lambda: self.gen(idx))
        self.emit(f'ADD R{rp}, R{rp}, R{ri}')
        self.free(ri)
        return rp

    def g_index(self, e, want):
        if e.kind == 'deref' and self.post_inc_var(e.a):
            r = self.alloc()
            ra = self.alloc()
            sym = e.a.a.sym
            self.emit(f'LOAD R{ra}, {sym.addr}')
            self.emit(f'LOADRP R{r}, R{ra}')
            self.emit(f'STORE R{ra}, {sym.addr}')
            self.free(ra)
            return r
        a = abs_addr(e)
        if a is not None:
            r = self.alloc()
            self.emit(f'LOAD R{r}, {a}')
            return r
        r = self.addr_reg(e)
        self.emit(f'LOADR R{r}, R{r}')
        return r

    g_deref = g_index

    def g_addr(self, e, want):
        return self.addr_reg(e.a)

    def post_inc_var(self, node):
        return (node.kind == 'incdec' and not node.prefix and node.op == '++'
                and node.a.kind == 'var' and node.a.ty.isptr())

    def g_assign(self, e, want):
        lv = e.a
        if lv.kind == 'deref' and self.post_inc_var(lv.a):
            rv = self.gen(e.b)
            ra = self.alloc()
            sym = lv.a.a.sym
            self.emit(f'LOAD R{ra}, {sym.addr}')
            self.emit(f'STORERP R{rv}, R{ra}')
            self.emit(f'STORE R{ra}, {sym.addr}')
            self.free(ra)
            return rv
        a = abs_addr(lv)
        if a is not None:
            rv = self.gen(e.b)
            self.emit(f'STORE R{rv}, {a}')
        else:
            ra, rv = self.pair(lambda: self.addr_reg(lv), lambda: self.gen(e.b))
            self.emit(f'STORER R{rv}, R{ra}')
            self.free(ra)
        return rv

    def g_incdec(self, e, want):
        lv = e.a
        ins = 'INC' if e.op == '++' else 'DEC'
        a = abs_addr(lv)
        if a is not None:
            r = self.alloc()
            self.emit(f'LOAD R{r}, {a}')
            if want and not e.prefix:
                self.emit(f'MOV R{TMP}, R{r}')
                self.emit(f'{ins} R{TMP}')
                self.emit(f'STORE R{TMP}, {a}')
            else:
                self.emit(f'{ins} R{r}')
                self.emit(f'STORE R{r}, {a}')
            return r
        ra = self.addr_reg(lv)
        self.emit(f'LOADR R{TMP}, R{ra}')
        if want and not e.prefix:
            self.emit(f'MOV R{TMP2}, R{TMP}')
            self.emit(f'{ins} R{TMP}')
            self.emit(f'STORER R{TMP}, R{ra}')
            self.emit(f'MOV R{ra}, R{TMP2}')
        else:
            self.emit(f'{ins} R{TMP}')
            self.emit(f'STORER R{TMP}, R{ra}')
            self.emit(f'MOV R{ra}, R{TMP}')
        return ra

    def emit_runtime(self):
        if 'RT_SDIV' in self.runtime:
            self.out.extend(f'    {x}' if not x.endswith(':') else x for x in [
                'RT_SDIV:', 'XOR R2, R0, R1', 'MOVI R3, 15', 'SHR R2, R2, R3',
                'ABS R0, R0', 'ABS R1, R1', 'DIV R0, R0, R1',
                'COMPI R2, 0', 'JE RT_SDIV_END', 'MOVI R3, 0', 'SUB R0, R3, R0',
                'RT_SDIV_END:', 'RTS'])
        if 'RT_SMOD' in self.runtime:
            self.out.extend(f'    {x}' if not x.endswith(':') else x for x in [
                'RT_SMOD:', 'MOV R2, R0', 'ABS R0, R0', 'ABS R1, R1', 'MOD R0, R0, R1',
                'MOVI R3, 15', 'SHR R2, R2, R3',
                'COMPI R2, 0', 'JE RT_SMOD_END', 'MOVI R3, 0', 'SUB R0, R3, R0',
                'RT_SMOD_END:', 'RTS'])
        if 'RT_SAR' in self.runtime:
            self.out.extend(f'    {x}' if not x.endswith(':') else x for x in [
                'RT_SAR:', 'MOVI R2, 65535', 'SHR R3, R2, R1', 'NOT R3, R3',
                'SHR R4, R0, R1', 'MOVI R5, 15', 'SHR R5, R0, R5',
                'COMPI R5, 0', 'JE RT_SAR_END', 'OR R4, R4, R3',
                'RT_SAR_END:', 'MOV R0, R4', 'RTS'])


def count_words(lines):
    total = 0
    for ln in lines:
        s = ln.strip()
        if not s or s.endswith(':'):
            continue
        parts = s.split(None, 1)
        ops = [t for t in re.split(r'[,\s]+', parts[1]) if t] if len(parts) > 1 else []
        total += 2 if any(not re.fullmatch(r'R[0-7]', t) for t in ops) else 1
    return total


def trim_data(words):
    words = list(words)
    while words and words[-1] == 0:
        words.pop()
    return words


def compile_source(src):
    toks = preprocess(lex(src))
    first = Parser(toks)
    offsets, total = first.frame_offsets(first.parse_program())
    parser = Parser(toks, offsets)
    main = parser.parse_program()
    if DATA_BASE + len(parser.data) > FRAME_TOP - total:
        raise CompileError("data and local variables do not fit in the data region", 0)
    gen = Gen(parser)
    lines = gen.compile(main)
    words = count_words(lines)
    if words > CODE_LIMIT:
        raise CompileError(f"code is too large ({words} words, limit {CODE_LIMIT})", 0)
    return lines, trim_data(parser.data), parser.warnings


def main(argv=None):
    ap = argparse.ArgumentParser(description="Organ 16 C compiler")
    ap.add_argument('source')
    ap.add_argument('-o', '--outdir')
    ap.add_argument('--no-assemble', action='store_true')
    args = ap.parse_args(argv)

    src_path = args.source
    name = os.path.basename(src_path)
    stem = os.path.splitext(name)[0]
    outdir = args.outdir or os.path.dirname(os.path.abspath(src_path))
    os.makedirs(outdir, exist_ok=True)

    try:
        with open(src_path, 'r') as f:
            src = f.read()
    except OSError as e:
        print(f"{name}: error: {e.strerror}", file=sys.stderr)
        return 1

    try:
        lines, data, warnings = compile_source(src)
    except CompileError as e:
        print(f"{name}:{e.line}: error: {e.msg}", file=sys.stderr)
        return 1

    org = os.path.join(outdir, stem + '.org')
    with open(org, 'w') as f:
        f.write('\n'.join(lines) + '\n')

    segments = [{stem + '.org': '0x0000'}]
    if data:
        with open(os.path.join(outdir, stem + '.data.bin'), 'w') as f:
            for i in range(0, len(data), 16):
                f.write(' '.join(f'{w:04x}' for w in data[i:i + 16]) + '\n')
        segments.append({stem + '.data.bin': f'0x{DATA_BASE:04X}'})
    linker = os.path.join(outdir, stem + '.l')
    with open(linker, 'w') as f:
        f.write('{\n    "segments": [\n')
        f.write(',\n'.join('        ' + '{' + f'"{k}": "{v}"' + '}' for s in segments for k, v in s.items()))
        f.write('\n    ]\n}\n')

    for line, msg in warnings:
        print(f"{name}:{line}: warning: {msg}", file=sys.stderr)

    if args.no_assemble:
        return 0
    assembler = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'compiler.py')
    res = subprocess.run([sys.executable, assembler, linker], capture_output=True, text=True)
    if res.returncode != 0 or 'Successfully' not in res.stdout:
        sys.stderr.write(res.stdout + res.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
