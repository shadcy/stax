#!/usr/bin/env python3
import sys
from PIL import Image, ImageFont, ImageDraw

def generate_font_c():
    font_path = "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf"
    font_size = 14
    try:
        font = ImageFont.truetype(font_path, font_size)
    except Exception as e:
        print(f"Error loading font {font_path}: {e}")
        return

    # Dimensions
    GLYPH_H = 16
    GLYPH_MAX_W = 12

    out_c = []
    out_c.append("/* ============================================================================")
    out_c.append(" * STAX — ubuntu_font_data.c")
    out_c.append(" * Authentic Canonical Ubuntu Font Glyph Atlas (Anti-Aliased 8-bit Alpha)")
    out_c.append(" * ============================================================================ */")
    out_c.append("")
    out_c.append("#include <stdint.h>")
    out_c.append("#include \"font.h\"")
    out_c.append("")
    out_c.append("typedef struct {")
    out_c.append("    uint8_t width;")
    out_c.append("    uint8_t height;")
    out_c.append("    uint8_t advance;")
    out_c.append("    const uint8_t *alpha;")
    out_c.append("} ubuntu_glyph_t;")
    out_c.append("")

    glyph_arrays = []
    glyph_entries = []

    # ASCII 32 to 126
    for c in range(32, 127):
        char_str = chr(c)
        
        # Measure character bounding box & advance
        bbox = font.getbbox(char_str) # (left, top, right, bottom)
        length = font.getlength(char_str)
        advance = max(int(round(length)), 3)
        if c == 32: # space
            advance = 5

        # Render glyph onto a grayscale image
        img = Image.new("L", (GLYPH_MAX_W, GLYPH_H), color=0)
        draw = ImageDraw.Draw(img)
        # Position with standard baseline offset (Y=1)
        draw.text((0, 1), char_str, font=font, fill=255)
        
        # Find trimmed width
        w = max(advance, 4)
        if w > GLYPH_MAX_W:
            w = GLYPH_MAX_W

        array_name = f"glyph_alpha_{c}"
        pixels = []
        for y in range(GLYPH_H):
            for x in range(w):
                val = img.getpixel((x, y))
                pixels.append(f"{val:3d}")
        
        arr_str = f"static const uint8_t {array_name}[] = {{\n  " + ", ".join(pixels) + "\n};"
        glyph_arrays.append(arr_str)
        glyph_entries.append(f"  [{c}] = {{ .width = {w}, .height = {GLYPH_H}, .advance = {advance}, .alpha = {array_name} }}")

    out_c.extend(glyph_arrays)
    out_c.append("")
    out_c.append("const ubuntu_glyph_t ubuntu_font_glyphs[128] = {")
    out_c.append(",\n".join(glyph_entries))
    out_c.append("};")
    out_c.append("")

    with open("/home/shreyash/Desktop/stax/gfx/ubuntu_font_data.c", "w") as f:
        f.write("\n".join(out_c))

    print("Successfully generated /home/shreyash/Desktop/stax/gfx/ubuntu_font_data.c")

if __name__ == '__main__':
    generate_font_c()
