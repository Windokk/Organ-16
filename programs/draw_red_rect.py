import compiler

if __name__ == "__draw_red_rect__":
    segments = [
        ("first_test.pglu", 0x0000)
    ]
    compiler.CompileMultiple(segments, "draw.bin")