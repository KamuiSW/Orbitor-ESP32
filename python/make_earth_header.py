from PIL import Image
import sys

inp = sys.argv[1]
out = sys.argv[2]
W = int(sys.argv[3])
H = int(sys.argv[4])
name = sys.argv[5]

img = Image.open(inp).convert("RGB")
img = img.resize((W, H), Image.LANCZOS)

def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

data = []
for y in range(H):
    for x in range(W):
        r, g, b = img.getpixel((x, y))
        data.append(rgb565(r, g, b))

with open(out, "w", encoding="utf-8") as f:
    f.write("#pragma once\n")
    f.write("#include <stdint.h>\n")
    f.write("#include <pgmspace.h>\n\n")
    f.write(f"#define {name.upper()}_W {W}\n")
    f.write(f"#define {name.upper()}_H {H}\n\n")
    f.write(f"static const uint16_t {name}[{W*H}] PROGMEM = {{\n")
    for i, v in enumerate(data):
        f.write(f"0x{v:04X},")
        if (i + 1) % 12 == 0:
            f.write("\n")
    f.write("\n};\n")

print("Wrote", out)
