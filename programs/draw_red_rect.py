import compiler

if __name__ == "__main__":
    segments = [
        ("draw_red_rect.pglu", 0x0000)
    ]
    compiler.CompileMultiple(segments, "draw.bin")
