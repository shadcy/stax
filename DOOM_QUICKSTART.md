# DOOM Quick Start Guide

## TL;DR - Get DOOM Running Now!

### For ASCII DOOM (Text-based)

```bash
# Build
make clean && make all

# Run
make qemu

# In TIOS shell, type:
doom
```

### For Graphical DOOM (Real 3D Graphics)

```bash
# Build
make clean && make all

# Run with graphics
make qemu-gfx

# In TIOS shell, type:
doomgfx
```

## Controls (Both Versions)

| Key | Action |
|-----|--------|
| W | Move forward |
| S | Move backward |
| A | Turn left |
| D | Turn right |
| Q | Quit to shell |
| M | Mini-map (ASCII only) |

## Which Version Should I Use?

### Use ASCII DOOM (`doom`) if:
- ✅ You want quick testing
- ✅ You're running in headless mode
- ✅ You want to see the text-based raycasting
- ✅ You're on a slow machine

### Use Graphical DOOM (`doomgfx`) if:
- ✅ You want real 3D graphics
- ✅ You want to see actual pixel rendering
- ✅ You want the full DOOM experience
- ✅ You can run QEMU with graphics

## Screenshots

### ASCII DOOM
```
@@@@@@                             
@@@@@@@@@                           
@@@@@@@@@@                          
@@@@@@@@@@          @               
@@@@@@@@@@%%%@@@@@@@@@@@@@@@%%%     
@@@@@@@@@@%%%@@@@@@@@@@@@@@@%%%%%%%%
@@@@@@@@@@%%%@@@@@@@@@@@@@@@%%%%%%%%
```

### Graphical DOOM
- 640x480 pixel display
- Red walls with depth shading
- Gray ceiling and dark floor
- Smooth 3D perspective

## Common Issues

**Q: Nothing happens when I press keys**
- A: Wait for the view to render first, then press keys

**Q: Graphics window doesn't open**
- A: Use `make qemu-gfx` not `make qemu`

**Q: Can't see anything**
- A: You might be facing a wall. Press S to back up

**Q: How do I exit QEMU?**
- A: Press Q in game, then Ctrl-A followed by X (for text mode) or Ctrl-C (for graphics mode)

## What's Next?

After playing DOOM, try:
- `snake` - Play Snake game
- `help` - See all available commands
- `status` - Check system info
- `mem` - View memory usage

## Want to Modify DOOM?

Source files:
- **ASCII version:** `games/doom.c`
- **Graphical version:** `games/doom_gfx.c`
- **Framebuffer driver:** `drivers/framebuffer.c`
- **Map data:** Edit the `map[][]` array in the source files

After editing, rebuild:
```bash
make clean && make all
```

## Documentation

- `docs/doom_guide.md` - Detailed ASCII DOOM guide
- `docs/graphical_doom.md` - Graphical DOOM technical details
- `doom/README.md` - ASCII DOOM implementation details

## Have Fun!

You're now running DOOM on a bare-metal ARM operating system you built yourself. Pretty cool! 🎮
