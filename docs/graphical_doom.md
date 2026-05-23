# Graphical DOOM for TIOS

## Overview

TIOS now includes TWO versions of DOOM:

1. **ASCII DOOM** (`doom` command) - Text-based raycasting using ASCII characters
2. **Graphical DOOM** (`doomgfx` command) - True pixel-based 3D rendering using the framebuffer

## Running Graphical DOOM

### Step 1: Build the OS

```bash
make clean
make all
```

### Step 2: Run QEMU with Graphics Support

**Important:** You need to run QEMU with graphics enabled (not in `-nographic` mode):

```bash
make qemu-gfx
```

This will open a QEMU window with:
- A graphical display (640x480 resolution)
- Serial console for text input/output

### Step 3: Launch Graphical DOOM

At the TIOS prompt, type:

```
tios> doomgfx
```

### Step 4: Play!

- **W** - Move forward
- **S** - Move backward  
- **A** - Turn left (10 degrees)
- **D** - Turn right (10 degrees)
- **Q** - Quit back to shell

## What You'll See

The graphical version renders a true 3D perspective with:

- **Red walls** with depth shading (darker = farther away)
- **Gray ceiling** (top half of screen)
- **Dark gray floor** (bottom half of screen)
- **Smooth raycasting** at 640x480 resolution
- **Side shading** - Different brightness for horizontal vs vertical walls

## Technical Details

### Framebuffer Driver

The graphical DOOM uses the **PL110 CLCD** (Color LCD Controller) available on the VersatilePB board:

- **Resolution:** 640x480 pixels
- **Color depth:** 16-bit (RGB565 format)
- **Framebuffer location:** 0x00200000 (2MB mark in RAM)
- **Memory usage:** ~600KB for framebuffer

### Graphics Pipeline

1. **Ray Casting:** 640 rays cast per frame (one per screen column)
2. **Distance Calculation:** Each ray marches until hitting a wall
3. **Wall Height:** Calculated based on distance (closer = taller)
4. **Color Selection:** Based on distance and wall orientation
5. **Pixel Drawing:** Direct framebuffer manipulation

### Color Scheme

Walls use red shading based on distance:
- Very close: `rgb(255, 0, 0)` - Bright red
- Close: `rgb(200, 0, 0)`
- Medium: `rgb(150, 0, 0)`
- Far: `rgb(100, 0, 0)`
- Very far: `rgb(50, 0, 0)` - Dark red

Vertical walls are slightly brighter than horizontal walls for depth perception.

### Map Layout

The graphical version uses a larger 16x16 map:

```
################
#              #
#              #
#   ###    ### #
#   #        # #
#   #        # #
#       ##     #
#       ##     #
#              #
#              #
#   #        # #
#   #        # #
#   ###    ### #
#              #
#              #
################
```

Player starts at position (8, 8) facing east.

## Comparison: ASCII vs Graphical

| Feature | ASCII DOOM | Graphical DOOM |
|---------|------------|----------------|
| Resolution | 40x20 characters | 640x480 pixels |
| Colors | 7 ASCII chars | 65,536 colors (16-bit) |
| Map size | 8x8 | 16x16 |
| FOV | 60 degrees | 60 degrees |
| Rays per frame | 40 | 640 |
| Display | UART console | PL110 framebuffer |
| QEMU mode | `-nographic` | Graphics window |

## Performance

The graphical version is more computationally intensive:
- 16x more rays to cast (640 vs 40)
- Direct pixel manipulation (~307,200 pixels)
- More complex distance calculations

On QEMU, expect some lag between key presses and screen updates.

## Troubleshooting

### Problem: No graphics window appears

**Solution:** Make sure you're using `make qemu-gfx` instead of `make qemu`. The regular `qemu` target uses `-nographic` mode.

### Problem: Screen is black

**Solution:** The framebuffer initialization might have failed. Check the serial console for error messages.

### Problem: Graphics are garbled

**Solution:** The PL110 CLCD might not be properly configured. This is rare on VersatilePB but can happen if QEMU version is very old.

### Problem: Input doesn't work

**Solution:** Make sure you're typing in the serial console window, not the graphics window. Input comes from the serial port.

### Problem: Very slow performance

**Solution:** This is expected on emulated ARM. The raycasting algorithm is computationally intensive. Try:
- Using a faster host machine
- Enabling KVM if on Linux: `qemu-system-arm -enable-kvm` (if supported)
- Reducing the map complexity

## Future Enhancements

Possible improvements:
- **Textures:** Load bitmap textures from FAT filesystem
- **Sprites:** Add enemies and objects
- **Lighting:** Dynamic lighting effects
- **Weapons:** Implement shooting mechanics
- **Multiple levels:** Load different maps
- **Better performance:** Optimize raycasting algorithm
- **Mouse support:** Add mouse look
- **Sound:** Use timer for simple beep effects

## Running Real DOOM

To run the **actual original DOOM** (not this simplified version), you would need:

1. **More memory:** At least 8-16MB RAM
2. **Better CPU:** Original DOOM needed 386+ (we have ARM926EJ-S which is capable)
3. **DOOM WAD files:** The game data files
4. **Port of DOOM engine:** Like Chocolate DOOM or PrBoom
5. **More drivers:** Keyboard, mouse, sound
6. **C library:** Full libc implementation

Projects like **doomgeneric** make this possible - they provide a minimal DOOM port that only requires:
- `DG_Init()` - Initialize
- `DG_DrawFrame()` - Draw 320x200 frame
- `DG_SleepMs()` - Sleep
- `DG_GetTicksMs()` - Get time
- `DG_GetKey()` - Get input

This could be a future project for TIOS!

## References

- [Lode's Raycasting Tutorial](https://lodev.org/cgtutor/raycasting.html)
- [PL110 Technical Reference Manual](https://developer.arm.com/documentation/ddi0161/latest/)
- [doomgeneric - Minimal DOOM port](https://github.com/ozkl/doomgeneric)
- [VersatilePB QEMU Documentation](https://www.qemu.org/docs/master/system/arm/versatile.html)

## Credits

- Original DOOM by id Software (1993)
- Raycasting technique inspired by Wolfenstein 3D
- Built for TIOS educational operating system
