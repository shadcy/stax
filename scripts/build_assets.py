#!/usr/bin/env python3
import os
import sys
import math
try:
    from PIL import Image
except ImportError:
    print("Error: Pillow required. pip install Pillow")
    sys.exit(1)

# Generate the same palette as STAX
palette = [(0,0,0)] * 256
for i in range(1, 16): palette[i] = (i*16, i*16, i*16)
for i in range(16, 32): palette[i] = (0, (i-16)*16, 0)
for i in range(32, 48): palette[i] = ((i-32)*16, 0, (i-32)*16)
for i in range(48, 64): palette[i] = (0, (i-48)*16, (i-48)*16)
for i in range(64, 80): palette[i] = ((i-64)*16, 0, 0)
for i in range(80, 96): palette[i] = ((i-80)*16, (i-80)*16, 0)
palette[253] = (0x23, 0xFE, 0x7C)
palette[254] = (0xEE, 0x3B, 0x48)
palette[255] = (255, 255, 255)

def closest_color(r, g, b):
    best_dist = float('inf')
    best_idx = 0
    # skip index 0 unless exactly 0,0,0 because 0 is transparent
    for i in range(1, 256):
        pr, pg, pb = palette[i]
        d = (pr-r)**2 + (pg-g)**2 + (pb-b)**2
        if d < best_dist:
            best_dist = d
            best_idx = i
    return best_idx

def process_image(img_path, var_name, target_size=None):
    print(f"Processing {img_path} -> {var_name} (resize: {target_size})")
    img = Image.open(img_path).convert('RGBA')
    if target_size:
        img = img.resize(target_size, Image.Resampling.LANCZOS)
    
    w, h = img.size
    
    out = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = img.getpixel((x, y))
            if a < 128:
                out.append(0)
            elif (r,g,b) == (0,0,0):
                # map exact black to index 0 (transparency for sprites) 
                # or maybe index 1 for actual black? Let's use 0 for full black to be safe, 
                # or index 1 if it needs to be opaque.
                out.append(0)
            else:
                out.append(closest_color(r, g, b))
    
    # generate C array
    c_str = f"const int {var_name}_width = {w};\n"
    c_str += f"const int {var_name}_height = {h};\n"
    c_str += f"const uint8_t {var_name}_data[] = {{\n"
    
    for i in range(0, len(out), 16):
        chunk = out[i:i+16]
        c_str += "    " + ", ".join(f"{x:3d}" for x in chunk) + ",\n"
    
    c_str += "};\n"
    return c_str

def main():
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    docs_img_dir = os.path.join(base_dir, "docs", "images")
    out_file = os.path.join(base_dir, "games", "assets.h")
    
    if not os.path.exists(docs_img_dir):
        print(f"Error: {docs_img_dir} does not exist.")
        sys.exit(1)
        
    c_code = "#ifndef GAMES_ASSETS_H\n#define GAMES_ASSETS_H\n\n"
    c_code += "#include <stdint.h>\n\n"
    
    # Process files
    slime_path = os.path.join(docs_img_dir, "slime.png")
    if os.path.exists(slime_path):
        c_code += process_image(slime_path, "spr_slime", target_size=(16, 16)) + "\n"
        
    tos_path = os.path.join(docs_img_dir, "STAX-engine.png")
    if os.path.exists(tos_path):
        c_code += process_image(tos_path, "spr_tos_engine") + "\n"
        
    c_code += "#endif\n"
    
    with open(out_file, "w") as f:
        f.write(c_code)
    print(f"Assets generated successfully at {out_file}")

if __name__ == "__main__":
    main()
