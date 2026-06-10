<div align="center">
  <h1>T-OS (Tios)</h1>
  
  <p>
    <b>A lightweight, custom bare-metal operating system built from scratch for ARM devices.</b>
  </p>
  
  <p>
    <img src="https://img.shields.io/badge/Architecture-ARM926EJ--S-blue.svg?style=flat-square" alt="Architecture">
    <img src="https://img.shields.io/badge/Language-C%20%7C%20Assembly-orange.svg?style=flat-square" alt="Language">
    <img src="https://img.shields.io/badge/Platform-QEMU-lightgrey.svg?style=flat-square" alt="Platform">
    <img src="https://img.shields.io/badge/License-MIT-green.svg?style=flat-square" alt="License">
  </p>
</div>

---

**T-OS (Tios)** is a lightweight, bare-metal operating system built from scratch for the ARM architecture. Designed as an experimental systems programming project, T-OS implements foundational kernel mechanics without relying on existing third-party abstractions.

The project demonstrates a complete vertical stack—from a custom assembly bootloader and low-level hardware drivers to a preemptive task scheduler, a page-based memory allocator, and a fully composited graphical window manager. T-OS enforces a clean architectural separation, decoupling hardware interfaces from core kernel logic and user-space applications.

## Core Architecture & Features

- **Boot Sequence & Initialization:** Features a custom assembly bootloader that establishes the initial stack pointer, configures ARM CPU operating modes, sets up the interrupt vector table, and securely hands off execution to the C-based kernel.
- **Memory Management (MMU):** Implements a robust page-based memory allocator to map virtual addresses to physical RAM, alongside a custom kernel heap manager (`kmalloc`/`kfree`) to handle dynamic memory allocation safely without leaks.
- **Interrupts & Task Scheduling:** Utilizes the PL190 Vectored Interrupt Controller (VIC) and SP804 Timer to drive a preemptive, multi-tasking scheduler. The kernel can manage concurrent processes, yielding and distributing CPU cycles dynamically.
- **Hardware Drivers:** Bare-metal, from-scratch driver implementations for the ARM VersatilePB board, interfacing directly with memory-mapped I/O registers for the PL050 (Keyboard/Mouse interface) and PL110 (Color Framebuffer).
- **Storage & Filesystem:** Integrates the FAT16 filesystem layer on top of a PL181 SD card block driver. This allows the OS to persist user data, read game assets, and manage file I/O operations reliably.
- **Networking:** Foundational integration with the lwIP networking stack for Ethernet frame transmission, packet handling, and DHCP IP assignment.
- **Windowing System:** A lightweight, compositing Window Manager built directly on the kernel's framebuffer abstraction. It features double-buffering to prevent tearing and provides a clean API for user-space applications to draw to the screen.

## Screenshots

### Desktop & Window Management
![Desktop Environment](readme-assets/desktop.png)

### Built-in Applications

**File Manager**
<br>
![File Manager](readme-assets/file-mgr.png)

**Text Editor**
<br>
![Text Editor](readme-assets/txt-editor.png)

**Memory Viewer**
<br>
![Memory Viewer](readme-assets/mem-viewer.png)

**Calculator**
<br>
![Calculator](readme-assets/apps-calc.png)

### Engine Execution (DOOM)

**DOOM**
<br>
![DOOM](readme-assets/game-doom.gif)

**Slime Escape**
<br>
![Slime Escape](readme-assets/game-slime.gif)

## Requirements

To compile and execute T-OS locally, you will need the following tools installed on your system:
- **ARM GCC Toolchain:** `arm-none-eabi-gcc`, `arm-none-eabi-ld`, `arm-none-eabi-objcopy`
- **QEMU:** `qemu-system-arm` (Specifically targeting the `versatilepb` machine profile)
- **Make:** GNU Make for the build system.

## Build & Run

It is straightforward to compile the OS from source. First, build the kernel:

```bash
make clean
make
```

To run the OS in the emulator with full graphics support:

```bash
make qemu-gfx
```

## Technical Stack

- **C:** Core kernel logic, memory management, and user-space applications.
- **ARM Assembly:** Bootloader, hardware initialization, and low-level context switching.
- **QEMU:** Emulation and hardware virtualization.
- **lwIP:** TCP/IP stack for networking.
- **FATFS:** Filesystem abstraction.

## Thanks

Special thanks to Cursor Agent for assisting with the debugging of complex linker issues, memory corruption bugs, and random kernel faults during the development cycle.

## Contact

- **LinkedIn:** [Shreyash Wanjari](https://in.linkedin.com/in/shreyashwanjari)
- **Email:** shreyashwanjari5162@gmail.com

## Repository

[https://github.com/shadcy/TOS](https://github.com/shadcy/TOS)
