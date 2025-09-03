import compiler

if __name__ == "__main__":
    segments = [
        ("draw_red_rect.org", 0x0000)
    ]
    compiler.CompileMultiple(segments, "draw.bin")
