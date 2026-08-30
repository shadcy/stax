#!/usr/bin/env python3
import sys
from PIL import Image, ImageFont, ImageDraw

def generate_font_c():
    sans_path = "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf"
    mono_path = "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf"
    
    try:
        font_sans = ImageFont.truetype(sans_path, 13)
        font_mono = ImageFont.truetype(mono_path, 14)
    except Exception as e:
        print(f"Error loading fonts: {e}")
        return

    GLYPH_H = 16
    GLYPH_MAX_W = 14

    out_c = []
    out_c.append("/* ============================================================================")
    out_c.append(" * STAX — ubuntu_font_data.c")
    out_c.append(" * Authentic Canonical Ubuntu Font Glyph Atlas (Sans & Mono)")
    out_c.append(" * Anti-Aliased 8-bit Alpha with Perfect Kerning, Tracking & Spacing")
    out_c.append(" * ============================================================================ */")
    out_c.append("")
    out_c.append("#include <stdint.h>")
    out_c.append("#include \"ubuntu_font_data.h\"")
    out_c.append("")

    # --- 1. Ubuntu Sans (Proportional) ---
    glyph_arrays_sans = []
    glyph_entries_sans = []

    for c in range(32, 127):
        char_str = chr(c)
        length = font_sans.getlength(char_str)
        advance = max(int(round(length)), 3)
        if c == 32:
            advance = 4

        img = Image.new("L", (GLYPH_MAX_W, GLYPH_H), color=0)
        draw = ImageDraw.Draw(img)
        draw.text((0, -1), char_str, font=font_sans, fill=255)
        
        bbox = font_sans.getbbox(char_str)
        if bbox:
            w = max(bbox[2], advance)
        else:
            w = advance
        
        if w > GLYPH_MAX_W: w = GLYPH_MAX_W
        if w < 1: w = 1

        array_name = f"glyph_sans_{c}"
        pixels = []
        for y in range(GLYPH_H):
            for x in range(w):
                val = img.getpixel((x, y))
                pixels.append(f"{val:3d}")
        
        arr_str = f"static const uint8_t {array_name}[] = {{\n  " + ", ".join(pixels) + "\n};"
        glyph_arrays_sans.append(arr_str)
        glyph_entries_sans.append(f"  [{c}] = {{ .width = {w}, .height = {GLYPH_H}, .advance = {advance}, .alpha = {array_name} }}")

    out_c.extend(glyph_arrays_sans)
    out_c.append("")
    out_c.append("const ubuntu_glyph_t ubuntu_font_glyphs[128] = {")
    out_c.append(",\n".join(glyph_entries_sans))
    out_c.append("};")
    out_c.append("")

    # --- 2. Ubuntu Mono (Fixed-Pitch 8px) ---
    glyph_arrays_mono = []
    glyph_entries_mono = []
    MONO_W = 8

    for c in range(32, 127):
        char_str = chr(c)
        img = Image.new("L", (MONO_W, GLYPH_H), color=0)
        draw = ImageDraw.Draw(img)
        draw.text((0, -1), char_str, font=font_mono, fill=255)

        array_name = f"glyph_mono_{c}"
        pixels = []
        for y in range(GLYPH_H):
            for x in range(MONO_W):
                val = img.getpixel((x, y))
                pixels.append(f"{val:3d}")

        arr_str = f"static const uint8_t {array_name}[] = {{\n  " + ", ".join(pixels) + "\n};"
        glyph_arrays_mono.append(arr_str)
        glyph_entries_mono.append(f"  [{c}] = {{ .width = {MONO_W}, .height = {GLYPH_H}, .advance = {MONO_W}, .alpha = {array_name} }}")

    out_c.extend(glyph_arrays_mono)
    out_c.append("")
    out_c.append("const ubuntu_glyph_t ubuntu_mono_glyphs[128] = {")
    out_c.append(",\n".join(glyph_entries_mono))
    out_c.append("};")
    out_c.append("")

    with open("/home/shreyash/Desktop/stax/gfx/ubuntu_font_data.c", "w") as f:
        f.write("\n".join(out_c))

    print("Successfully generated /home/shreyash/Desktop/stax/gfx/ubuntu_font_data.c with Sans & Mono fonts.")

if __name__ == '__main__':
    generate_font_c()
