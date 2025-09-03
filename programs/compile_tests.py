import compiler

if __name__ == "__tests__":
    segments = [
        ("first_test.org", 0x0000),
        ("second_test.org", 0x3000),
    ]
    compiler.CompileMultiple(segments, "out.bin")