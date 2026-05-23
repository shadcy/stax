# TIOS with DOOM - Complete Package

## 🎮 What You Get

Your TIOS operating system now includes:

1. **Full Graphical Display** - 640x480 framebuffer with text console
2. **ASCII DOOM** - Text-based 3D raycasting game
3. **Graphical DOOM** - Real pixel-based 3D rendering
4. **Dual Console** - Output to both serial and display
5. **Complete OS** - All running on bare-metal ARM

## 🚀 Quick Start (30 seconds)

```bash
# Build everything
make clean && make all

# Run with graphics
make qemu-gfx

# In TIOS, type:
doomgfx
```

**That's it!** You're now running DOOM on your own operating system!

## 📋 What's Included

### Files Added/Modified

**New Drivers:**
- `drivers/framebuffer.c` - PL110 LCD controller driver
- `drivers/gfx_console.c` - Graphical text console with 8x8 font

**New Games:**
- `games/doom.c` - ASCII DOOM (40x20 text mode)
- `games/doom_gfx.c` - Graphical DOOM (640x480 pixels)

**New Headers:**
- `include/framebuffer.h`
- `include/gfx_console.h`
- `include/doom.h`

**Modified:**
- `kernel/kernel.c` - Initialize graphics at boot
- `kernel/command.c` - Add doom, doomgfx, fbtest commands
- `drivers/console.c` - Dual output (serial + graphics)
- `makefile` - New qemu-gfx target

**Documentation:**
- `GRAPHICAL_MODE.md` - Complete graphical mode guide
- `DOOM_QUICKSTART.md` - Quick reference
- `TROUBLESHOOTING.md` - Common issues and fixes
- `docs/graphical_doom.md` - Technical details
- `docs/doom_guide.md` - ASCII DOOM guide

## 🎯 Three Ways to Run

### 1. Text-Only Mode (Serial Console)

```bash
make qemu
```

- No graphics window
- Serial terminal only
- Use `doom` command for ASCII DOOM
- Faster, simpler

### 2. Graphical Mode (Recommended)

```bash
make qemu-gfx
```

- QEMU graphics window (640x480)
- Serial terminal for input
- Everything displays on screen
- Use `doomgfx` for 3D DOOM
- **This is what you want!**

### 3. Debug Mode

```bash
make debug
# In another terminal:
make gdb
```

- For debugging with GDB
- Graphics enabled

## 🎮 Available Commands

Type these at the `tios>` prompt:

| Command | Description | Mode |
|---------|-------------|------|
| `help` | Show all commands | Both |
| `status` | System information | Both |
| `mem` | Memory usage | Both |
| `clear` | Clear screen | Both |
| `snake` | Play Snake game | Both |
| `doom` | ASCII DOOM (text) | Text mode |
| `doomgfx` | Graphical DOOM (3D) | Graphics mode |
| `fbtest` | Test framebuffer colors | Graphics mode |

## 🕹️ DOOM Controls

**Both versions:**
- **W** - Move forward
- **S** - Move backward
- **A** - Turn left
- **D** - Turn right
- **Q** - Quit to shell

**ASCII DOOM only:**
- **M** - Show mini-map

## 📊 Specifications

### ASCII DOOM
- 40×20 character display
- 8×8 map
- 7 distance levels (characters: # @ % + = .)
- 60° field of view
- 40 rays per frame

### Graphical DOOM
- 640×480 pixel display
- 16×16 map
- RGB565 color (65,536 colors)
- Red walls with distance shading
- 60° field of view
- 640 rays per frame
- Raycasting engine

### Display System
- **Resolution:** 640×480 pixels
- **Text mode:** 80×60 characters
- **Font:** 8×8 bitmap
- **Colors:** 16-bit RGB565
- **Framebuffer:** 600KB at 0x00200000
- **Controller:** PL110 CLCD

## 🔧 Build System

```bash
make clean      # Clean build directory
make all        # Build everything
make qemu       # Run text mode
make qemu-gfx   # Run graphics mode
make debug      # Run with GDB
make gdb        # Connect GDB
make dump       # Disassemble kernel
make size       # Show binary sizes
```

## 📁 Project Structure

```
tios/
├── boot/              # Bootloader
├── kernel/            # Kernel core
├── drivers/           # Hardware drivers
│   ├── console.c      # Dual console (serial + gfx)
│   ├── framebuffer.c  # LCD controller
│   └── gfx_console.c  # Text on framebuffer
├── games/             # Games
│   ├── snake.c        # Snake game
│   ├── doom.c         # ASCII DOOM
│   └── doom_gfx.c     # Graphical DOOM
├── include/           # Headers
├── docs/              # Documentation
├── makefile           # Build system
└── README_DOOM.md     # This file
```

## 🐛 Troubleshooting

### "Guest has not initialized the display"

**Fixed!** The framebuffer now initializes at boot automatically.

### No graphics window

Use `make qemu-gfx` not `make qemu`

### Can't see DOOM

1. Make sure you used `make qemu-gfx`
2. Type `fbtest` first to verify graphics work
3. Then try `doomgfx`

### Input doesn't work

Type in the **terminal window**, not the QEMU graphics window.

### More help

See `TROUBLESHOOTING.md` for detailed solutions.

## 📚 Documentation

- **GRAPHICAL_MODE.md** - How the graphical system works
- **DOOM_QUICKSTART.md** - Quick reference guide
- **TROUBLESHOOTING.md** - Common problems and solutions
- **docs/graphical_doom.md** - Technical implementation details
- **docs/doom_guide.md** - ASCII DOOM gameplay guide

## 🎓 What You Learned

By building this, you now understand:

✅ Framebuffer programming
✅ LCD controller configuration
✅ Bitmap font rendering
✅ Raycasting algorithms
✅ Fixed-point mathematics
✅ Memory-mapped I/O
✅ Dual console systems
✅ Game development on bare metal

## 🚀 Next Steps

Want to go further?

1. **Port real DOOM** - Use doomgeneric
2. **Add textures** - Load bitmaps from FAT filesystem
3. **Implement sprites** - Add enemies and objects
4. **Add sound** - Use timer for beeps
5. **Create GUI** - Build a window system
6. **Add mouse** - PS/2 mouse driver
7. **Network** - Add network stack for multiplayer!

## 🎉 Success!

You now have:
- ✅ A working operating system
- ✅ Graphical display support
- ✅ Two versions of DOOM
- ✅ Complete documentation
- ✅ A great portfolio project!

**Congratulations!** You're running DOOM on an OS you built from scratch! 🎮

---

## Quick Command Reference

```bash
# Build and run
make clean && make all && make qemu-gfx

# In TIOS
tios> fbtest      # Test display
tios> doomgfx     # Play DOOM
tios> help        # See all commands
```

**Have fun!** 🎮🚀
