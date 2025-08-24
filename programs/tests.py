import compiler

if __name__ == "__tests__":
    segments = [
        ("first_test.pglu", 0x0000),
        ("second_test.pglu", 0x3000),
    ]
    compiler.CompileMultiple(segments, "out.bin")