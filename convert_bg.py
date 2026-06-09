from PIL import Image
import struct
import sys

def convert_to_16bit_bmp(input_path, output_path, target_width=640, target_height=480):
    img = Image.open(input_path).convert('RGB')
    
    # Resize and crop to target_width x target_height
    img_ratio = img.width / img.height
    target_ratio = target_width / target_height
    
    if img_ratio > target_ratio:
        # Image is wider, crop width
        new_width = int(target_ratio * img.height)
        offset = (img.width - new_width) // 2
        img = img.crop((offset, 0, offset + new_width, img.height))
    else:
        # Image is taller, crop height
        new_height = int(img.width / target_ratio)
        offset = (img.height - new_height) // 2
        img = img.crop((0, offset, img.width, offset + new_height))
        
    img = img.resize((target_width, target_height), Image.Resampling.LANCZOS)
    
    width, height = img.size
    
    # Row size in bytes must be a multiple of 4
    row_size = ((width * 2) + 3) & ~3
    image_size = row_size * height
    
    file_size = 14 + 40 + image_size
    
    with open(output_path, 'wb') as f:
        # File Header (14 bytes)
        f.write(b'BM')
        f.write(struct.pack('<I', file_size))
        f.write(struct.pack('<H', 0))
        f.write(struct.pack('<H', 0))
        f.write(struct.pack('<I', 14 + 40)) # Offset to pixel array
        
        # Info Header (40 bytes)
        f.write(struct.pack('<I', 40)) # Size of info header
        f.write(struct.pack('<i', width))
        f.write(struct.pack('<i', height))
        f.write(struct.pack('<H', 1)) # Color planes
        f.write(struct.pack('<H', 16)) # BPP
        f.write(struct.pack('<I', 0)) # Compression (0 = BI_RGB)
        f.write(struct.pack('<I', image_size))
        f.write(struct.pack('<i', 0)) # x ppm
        f.write(struct.pack('<i', 0)) # y ppm
        f.write(struct.pack('<I', 0)) # Colors used
        f.write(struct.pack('<I', 0)) # Important colors
        
        # Pixel data (bottom-up)
        for y in range(height - 1, -1, -1):
            row_data = bytearray()
            for x in range(width):
                r, g, b = img.getpixel((x, y))
                # RGB565 format
                rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                row_data += struct.pack('<H', rgb565)
            # Padding
            row_data += b'\x00' * (row_size - len(row_data))
            f.write(row_data)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: convert_bg.py <input> <output>")
        sys.exit(1)
    convert_to_16bit_bmp(sys.argv[1], sys.argv[2])
