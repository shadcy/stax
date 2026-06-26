# DOOM Game Guide

## Quick Start

1. **Build the OS**:
   ```bash
   make clean
   make all
   ```

2. **Run in QEMU**:
   ```bash
   make qemu
   ```

3. **Launch DOOM**:
   At the STAX prompt, type:
   ```
   STAX> doom
   ```

4. **Play**:
   - Use `W` to move forward
   - Use `S` to move backward
   - Use `A` to turn left
   - Use `D` to turn right
   - Press `M` to see the mini-map
   - Press `Q` to quit

## What to Expect

You'll see a first-person ASCII view of a 3D maze. The view updates as you move and turn. Walls appear as ASCII characters, with closer walls using denser characters like `#` and `@`, while distant walls use lighter characters like `+` and `=`.

Example view:
```
                                        
                                        
        @@@@@@@@@@@@@@@@@@@@@@@@        
      @@@@@@@@@@@@@@@@@@@@@@@@@@@@      
    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@    
    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@    
  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@  
  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@  
  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@  
  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@  
  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@  
  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@  
  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@  
    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@    
    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@    
      @@@@@@@@@@@@@@@@@@@@@@@@@@@@      
        @@@@@@@@@@@@@@@@@@@@@@@@        
........................................
........................................
........................................
----------------------------------------
DOOM | W/S:Move A/D:Turn Q:Quit
```

## Tips

1. **Navigation**: The game uses a simple 8x8 grid map. You start in the center.

2. **Collision**: You cannot walk through walls. If you try to move into a wall, you'll stay in place.

3. **Orientation**: 
   - 0° = East (right)
   - 90° = South (down)
   - 180° = West (left)
   - 270° = North (up)

4. **Mini-map**: Press `M` to see where you are in the maze. Press any key to return to the game.

5. **Performance**: The game renders on each key press, so movement is discrete rather than continuous.

## Troubleshooting

**Problem**: Game doesn't respond to input
- **Solution**: Make sure you're pressing keys after the view renders. The game polls for input after each render.

**Problem**: Graphics look garbled
- **Solution**: Your terminal might not support ANSI escape codes. Try a different terminal emulator.

**Problem**: Game is too slow
- **Solution**: This is expected on the emulated ARM system. The raycasting calculations take time.

**Problem**: Can't see anything
- **Solution**: You might be facing a wall very closely. Try pressing `S` to back up or `A`/`D` to turn.

## Exit

Press `Q` at any time to quit the game and return to the STAX shell.

## Technical Notes

- The game uses a raycasting algorithm similar to Wolfenstein 3D and early DOOM
- All math is done using fixed-point arithmetic (no floating point)
- The field of view is 60 degrees
- Each frame casts 40 rays (one per screen column)
- The screen resolution is 40x20 characters
