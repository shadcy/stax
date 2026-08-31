================================================================================
  STAX OS — Shareware Edition (ARM926EJ-S / VersatilePB)
================================================================================

Welcome to STAX OS!
STAX is a modern 32-bit multitasking operating system featuring a compositing
window manager, virtual memory with demand paging, preemptive scheduler,
VFS with FatFs, high-performance networking stack, and standalone ELF-32 runtime.

--------------------------------------------------------------------------------
1. DESKTOP & WINDOW MANAGER
--------------------------------------------------------------------------------
- Top Taskbar:
  * Click 'STAX' menu for System Info, Terminal, Settings, and Reboot.
  * Click 'Apps' menu for quick access to all built-in GUI applications:
    - Web Browser      (HTML rendering, HTTP/HTTPS)
    - Terminal         (PTY master/slave console)
    - File Manager     (Directory navigation, file viewing)
    - Text Editor      (Lightweight note and code editing)
    - Calculator       (Full arithmetic & expression evaluator)
    - System Info      (Real-time hardware, memory, and kernel metrics)
    - Task Manager     (Process monitor and state tracking)
    - DOOM             (Full shareware graphical DOOM 3D game)
    - Settings         (Desktop themes, wallpapers, system prefs)

- Shortcuts:
  * Ctrl+Tab           : Modern window switcher HUD with arrow navigation
  * Mouse Drag         : Move window titlebars and desktop icons
  * Double Click Icon  : Open file or launch application

--------------------------------------------------------------------------------
2. STANDALONE APPLICATION PACKAGES (.LAUNCH & .ELF)
--------------------------------------------------------------------------------
STAX supports true standalone userland applications:
- /BIN/HELLO.ELF       : Standalone ELF-32 userland program.
- /BIN/DOOM.LAUNCH     : Packaged shareware DOOM runtime (ELF + WAD + metadata).

To launch from shell:
  run doom.launch      (or: game --doom)
  exec hello.elf

--------------------------------------------------------------------------------
3. COMMAND LINE INTERFACE (SHELL)
--------------------------------------------------------------------------------
Available commands in the STAX shell:
  help                 - Display available commands and usage
  clear                - Clear terminal screen
  status               - Show kernel version, CPU, and uptime
  tasks / ps           - View preemptive scheduler and task list
  uptime               - View system uptime, RAM, and heap statistics
  fs                   - Display FAT filesystem volume details
  vfs                  - Display Virtual File System mounts and open descriptors
  dev                  - Inspect /dev device nodes (/dev/null, /dev/urandom, etc.)
  ls [--size]          - List directory contents
  cd <dir>             - Change current working directory
  pwd                  - Print working directory
  cat <file>           - Display text file contents
  nano <file>          - Edit text file directly in console
  touch <file>         - Create empty file
  rm <file>            - Remove file or directory
  mkdir <dir>          - Create directory
  exec <binary.elf>    - Execute standalone ELF-32 binary in user mode
  run <app.launch>     - Launch application package (.launch)
  game --doom          - Launch graphical DOOM
  date / time          - Display real-time clock (IST UTC+05:30)
  ifconfig             - Show network interface & IP configuration
  ping <ip>            - Send ICMP echo requests to network hosts
  browser              - Open graphical web browser
  fwupdate <file.stax> - Update signed system firmware
  fwconfirm            - Confirm active firmware to prevent rollback
  reboot               - Restart system

--------------------------------------------------------------------------------
4. SHAREWARE DISTRIBUTION & INTEGRITY
--------------------------------------------------------------------------------
STAX includes cryptographic secure boot and firmware validation (Ed25519 &
SHA-256). All firmware slots and standalone packages are signed and verified
before execution.

Thank you for choosing STAX OS!
================================================================================
