import argparse
from PIL import Image

def rgb_to_rgb565(r, g, b):
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5

def rgb_to_rgb555(r, g, b):
    r5 = (r >> 3) & 0x1F
    g5 = (g >> 3) & 0x1F
    b5 = (b >> 3) & 0x1F
    return (r5 << 10) | (g5 << 5) | b5

def rgb_to_argb1555(r, g, b):
    a = 1  # always opaque
    r5 = (r >> 3) & 0x1F
    g5 = (g >> 3) & 0x1F
    b5 = (b >> 3) & 0x1F
    return (a << 15) | (r5 << 10) | (g5 << 5) | b5

def gray16(r, g, b):
    gray = int(0.299*r + 0.587*g + 0.114*b)
    value = (gray << 8) | gray  # 16-bit grayscale
    return value

def convert(img_path, out_path, fmt):
    img = Image.open(img_path).convert("RGB")
    width, height = img.size

    if width != height or width != 128:
        print("Error: width and height should both be 128px")
        return

    fmt = fmt.lower()
    hex_values = []

    for y in range(height):
        for x in range(width):
            r, g, b = img.getpixel((x, y))

            if fmt == "rgb565":
                pixel = rgb_to_rgb565(r, g, b)
            elif fmt == "rgb555":
                pixel = rgb_to_rgb555(r, g, b)
            elif fmt == "argb1555":
                pixel = rgb_to_argb1555(r, g, b)
            elif fmt == "gray16":
                pixel = gray16(r, g, b)
            else:
                raise ValueError("Unknown format: " + fmt)

            hex_values.append(f"{pixel:04X}")

    # Write 16 numbers per line
    with open(out_path, "w") as f:
        for i in range(0, len(hex_values), 16):
            line = " ".join(hex_values[i:i+16])
            f.write(line + "\n")

    print(f"Saved {out_path} ({fmt}, {width}x{height}) in hex format")

def main():
    parser = argparse.ArgumentParser(description="Convert image to 16-bit raw binary.")
    parser.add_argument("--input", required=True, help="Input image file")
    parser.add_argument("--output", required=True, help="Output .bin file")
    parser.add_argument("--format", default="rgb565",
                        choices=["rgb565", "rgb555", "argb1555", "gray16"],
                        help="16-bit pixel format")
    args = parser.parse_args()
    convert(args.input, args.output, args.format)

if __name__ == "__main__":
    main()