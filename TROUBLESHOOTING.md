# DOOM Troubleshooting Guide

## Problem: "DOOM initialized" but no display appears

This happens when you're trying to run the graphical version in text-only mode, or the ASCII version isn't rendering properly.

### Solution 1: Use the Correct QEMU Mode

**For ASCII DOOM (`doom` command):**
```bash
make qemu          # Text-only mode
```
Then type: `doom`

**For Graphical DOOM (`doomgfx` command):**
```bash
make qemu-gfx      # Graphics mode - opens a window
```
Then type: `doomgfx`

### Solution 2: Test Your Setup

1. **Exit QEMU** (Ctrl-A then X)

2. **Rebuild everything:**
   ```bash
   make clean
   make all
   ```

3. **For ASCII DOOM test:**
   ```bash
   make qemu
   ```
   In TIOS, type: `doom`
   
   You should see ASCII characters like `#`, `@`, `%` forming walls.

4. **For Graphical DOOM test:**
   ```bash
   make qemu-gfx
   ```
   
   First test the framebuffer: `fbtest`
   
   You should see colored rectangles in a QEMU window.
   
   Then try: `doomgfx`

## Common Issues

### Issue: "Nothing happens after typing doom"

**Cause:** The game is waiting for input after rendering.

**Solution:** 
- Wait 2-3 seconds for initialization
- Press W, A, S, or D to move
- The view should update

### Issue: "QEMU window doesn't open for doomgfx"

**Cause:** Using wrong make target.

**Solution:**
```bash
# Exit current QEMU (Ctrl-A then X)
make qemu-gfx     # NOT "make qemu"
```

### Issue: "Framebuffer initialization failed"

**Cause:** Running in `-nographic` mode.

**Solution:** Use `make qemu-gfx` which removes the `-nographic` flag.

### Issue: "Screen is all black in graphics mode"

**Cause:** Framebuffer might not be working.

**Solution:**
1. Type `fbtest` to test the framebuffer
2. You should see colored rectangles
3. If you don't see anything, check QEMU version:
   ```bash
   qemu-system-arm --version
   ```
   Should be 2.0 or higher.

### Issue: "ASCII DOOM shows garbled characters"

**Cause:** Terminal doesn't support ANSI escape codes.

**Solution:**
- Use a modern terminal (xterm, gnome-terminal, konsole)
- Or try running in a different terminal emulator

### Issue: "Input lag or no response"

**Cause:** Game is rendering or waiting for input.

**Solution:**
- Wait for the view to fully render
- Press keys slowly, one at a time
- Each key press triggers a new frame render

## Step-by-Step Debug Process

### For ASCII DOOM:

1. **Start QEMU:**
   ```bash
   make qemu
   ```

2. **Wait for TIOS prompt:**
   ```
   tios>
   ```

3. **Type doom:**
   ```
   tios> doom
   ```

4. **You should see:**
   ```
   Loading DOOM...
   Initializing raycaster...
   [ASCII graphics appear here]
   ----------------------------------------
   DOOM | W/S:Move A/D:Turn Q:Quit
   ```

5. **Press W to move forward** - view should change

6. **Press Q to quit**

### For Graphical DOOM:

1. **Start QEMU with graphics:**
   ```bash
   make qemu-gfx
   ```
   
   **Two windows should open:**
   - QEMU graphics window (640x480, initially black)
   - Terminal with serial console

2. **In the terminal, wait for:**
   ```
   tios>
   ```

3. **Test framebuffer first:**
   ```
   tios> fbtest
   ```
   
   **You should see colored rectangles in the QEMU window**

4. **If fbtest works, try DOOM:**
   ```
   tios> doomgfx
   ```

5. **You should see:**
   - Terminal: "Initializing graphical DOOM..."
   - QEMU window: Red walls, gray ceiling, dark floor

6. **Type in the terminal window** (not the graphics window)

7. **Press W/A/S/D to move**

8. **Press Q to quit**

## Quick Reference

| Command | QEMU Mode | What You See |
|---------|-----------|--------------|
| `doom` | `make qemu` | ASCII characters in terminal |
| `doomgfx` | `make qemu-gfx` | 3D graphics in QEMU window |
| `fbtest` | `make qemu-gfx` | Colored rectangles test |

## Still Not Working?

### Check Your Build:

```bash
# Clean and rebuild
make clean
make all

# Check kernel size
ls -lh build/kernel.bin

# Should be around 17-18 KB
```

### Check QEMU Installation:

```bash
# Check if QEMU is installed
which qemu-system-arm

# Check version
qemu-system-arm --version

# Should be 2.0 or higher
```

### Manual QEMU Test:

**For text mode:**
```bash
qemu-system-arm -M versatilepb -kernel build/bootloader.bin \
  -drive file=os.bin,if=sd,format=raw -nographic -serial mon:stdio
```

**For graphics mode:**
```bash
qemu-system-arm -M versatilepb -kernel build/bootloader.bin \
  -drive file=os.bin,if=sd,format=raw -serial stdio
```

## Expected Behavior

### ASCII DOOM (doom):
- Runs in terminal
- Uses characters: `#` `@` `%` `+` `=` `.`
- 40x20 character display
- Updates on each key press
- Works in `-nographic` mode

### Graphical DOOM (doomgfx):
- Opens QEMU window
- 640x480 pixel display
- Red walls with shading
- Gray ceiling, dark floor
- Input via terminal, display in window
- Requires graphics mode (no `-nographic`)

## Getting Help

If you're still stuck:

1. **Check which command you typed:** `doom` or `doomgfx`?
2. **Check which make target you used:** `make qemu` or `make qemu-gfx`?
3. **Try the other version** - if ASCII doesn't work, try graphical (or vice versa)
4. **Run `fbtest`** to verify graphics are working
5. **Check the serial console** for error messages

## Success Indicators

**ASCII DOOM working:**
```
@@@@@@                             
@@@@@@@@@                           
@@@@@@@@@@                          
```

**Graphical DOOM working:**
- QEMU window shows red walls
- Terminal shows "DOOM initialized"
- Pressing W/A/S/D changes the view

## Pro Tips

1. **Always match the command to the QEMU mode:**
   - `make qemu` → use `doom`
   - `make qemu-gfx` → use `doomgfx`

2. **Test framebuffer first:**
   - Before running `doomgfx`, run `fbtest`
   - If `fbtest` doesn't show colors, `doomgfx` won't work

3. **Be patient:**
   - Wait for initialization messages
   - Wait for the first frame to render
   - Then start pressing keys

4. **Use the right window:**
   - Type commands in the **terminal/console**
   - View graphics in the **QEMU window**
