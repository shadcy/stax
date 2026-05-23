# DOOM for TIOS

A bare minimum ASCII-based DOOM-like raycasting game for the TIOS operating system.

## Overview

This is a simplified 3D raycasting engine that renders a first-person view using ASCII characters in the console. It's inspired by the original DOOM but adapted for a text-only environment with limited resources.

## Features

- **Raycasting Engine**: Simple raycasting algorithm that creates a 3D-like perspective
- **ASCII Graphics**: Uses different ASCII characters to represent walls at varying distances
- **8x8 Map**: Navigate through a small maze with walls and open spaces
- **Real-time Movement**: Move forward/backward and rotate left/right
- **Mini-map**: Press 'M' to view your position on the map

## Controls

| Key | Action |
|-----|--------|
| W   | Move forward |
| S   | Move backward |
| A   | Rotate left (15 degrees) |
| D   | Rotate right (15 degrees) |
| M   | Show mini-map |
| Q   | Quit game |

## How to Play

1. Boot TIOS in QEMU:
   ```bash
   make qemu
   ```

2. At the TIOS prompt, type:
   ```
   tios> doom
   ```

3. Use WASD keys to navigate through the maze
4. Press Q to exit back to the shell

## Technical Details

### Graphics Rendering

The game uses different ASCII characters to represent depth:
- `#` - Very close walls
- `@` - Close walls
- `%` - Medium distance
- `+` - Far walls
- `=` - Very far walls
- `.` - Floor/distant objects

### Implementation

- **Fixed-point Math**: All calculations use fixed-point arithmetic (8-bit fractional part) to avoid floating-point operations
- **Custom Division**: Implements integer division without relying on ARM division libraries
- **Trigonometry Tables**: Pre-computed sine/cosine lookup tables for 360 degrees
- **Screen Buffer**: 40x20 character screen buffer rendered each frame
- **Collision Detection**: Simple grid-based collision detection

### Memory Usage

- Screen buffer: ~820 bytes (40x20 + null terminators)
- Trig tables: ~2880 bytes (360 * 4 * 2)
- Map data: 64 bytes (8x8)
- Total: ~4KB of static data

### Performance

The game runs in a polling loop, checking for input and re-rendering the view on each key press. The raycasting algorithm casts 40 rays (one per screen column) with up to 64 steps per ray.

## Map Layout

```
########
#      #
# #  # #
#      #
#  ##  #
#      #
# #  # #
########
```

Player starts in the center (position 3,3) facing east (0 degrees).

## Limitations

- Text-only graphics (no pixel-level rendering)
- Small 8x8 map size
- Simple collision detection
- No enemies or weapons (this is just the engine)
- Limited field of view (60 degrees)
- Approximate trigonometry (linear interpolation)

## Future Enhancements

Possible improvements for future versions:
- Larger maps
- Textured walls (different characters for different wall types)
- Enemies and basic AI
- Weapon system
- Multiple levels
- Sound effects (beeps via timer)
- Smoother rotation (smaller angle increments)
- Better trigonometry approximation

## Credits

Inspired by:
- id Software's DOOM (1993)
- Lode Vandevenne's raycasting tutorials
- ASCII-based raycasting demos

Built for TIOS - A lightweight ARM operating system for educational purposes.
