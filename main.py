import compiler

if __name__ == "__main__":
    segments = [
        ("first.pglu", 0x0000),
        ("second.pglu", 0x3000),
    ]
    compiler.CompileMultiple(segments, "out.bin")