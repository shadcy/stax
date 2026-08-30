# Contributing to STAX

Thank you for your interest in contributing to **STAX**! STAX is an open-source, bare-metal 32-bit operating system developed from scratch for the ARM architecture (ARM926EJ-S / VersatilePB). Whether you are fixing bugs, writing drivers, implementing new userland applications, improving documentation, or creating games, your contributions are welcome.

This guide provides everything you need to know to get started.

---

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How Can I Contribute?](#how-can-i-contribute)
- [Development Environment Setup](#development-environment-setup)
  - [Native Prerequisites (Linux / WSL2)](#native-prerequisites-linux--wsl2)
  - [Docker Setup](#docker-setup)
- [Building and Running STAX](#building-and-running-stax)
- [Project Architecture & Directory Layout](#project-architecture--directory-layout)
- [Coding Guidelines & Standards](#coding-guidelines--standards)
- [Creating Features](#creating-features)
  - [Adding a New Userland Application](#adding-a-new-userland-application)
  - [Adding or Modifying Drivers](#adding-or-modifying-drivers)
  - [Adding a .launch Application Package](#adding-a-launch-application-package)
- [Development Workflow](#development-workflow)
  - [1. Fork & Branch](#1-fork--branch)
  - [2. Commit Messages](#2-commit-messages)
  - [3. Testing & Verification](#3-testing--verification)
  - [4. Submitting a Pull Request](#4-submitting-a-pull-request)
- [Reporting Bugs & Requesting Features](#reporting-bugs--requesting-features)

---

## Code of Conduct

We are committed to providing a friendly, safe, and welcoming environment for all contributors. Please:
- Be respectful, constructive, and empathetic in all interactions.
- Provide thoughtful feedback and accept constructive criticism gracefully.
- Focus on what is best for the community and the codebase.

---

## How Can I Contribute?

There are many ways to contribute to STAX:

- 🎮 **Applications & Games**: Build new CLI utilities, GUI applications, or port retro games into `.launch` packages.
- ⚙️ **Kernel & Subsystems**: Improve memory management, task scheduling, IPC, signals, or POSIX compatibility.
- 🔌 **Hardware Drivers**: Add or improve device drivers (audio emulation, timers, storage, network interfaces).
- 🎨 **Window Manager & GUI**: Enhance desktop rendering, widget libraries, themes, font rendering, and window compositing.
- 🌐 **Networking**: Expand lwIP networking features, sockets, network protocols, or networking utilities.
- 🧪 **Testing & Benchmarks**: Expand `bench/` automated test suites and fault injection tests.
- 📖 **Documentation**: Improve guides, write architecture specs, add tutorials, or fix typos.

---

## Development Environment Setup

### Native Prerequisites (Linux / WSL2)

To build and run STAX natively, install the required toolchains and packages:

#### Ubuntu / Debian:
```bash
sudo apt update
sudo apt install -y \
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    libnewlib-arm-none-eabi \
    qemu-system-arm \
    build-essential \
    gcc \
    mtools \
    dosfstools \
    python3 \
    python3-pil \
    xxd
```

#### Arch Linux:
```bash
sudo pacman -S arm-none-eabi-gcc arm-none-eabi-binutils arm-none-eabi-newlib qemu-system-arm base-devel mtools dosfstools python python-pillow
```

#### macOS (via Homebrew):
```bash
brew tap osx-cross/arm
brew install arm-none-eabi-gcc qemu mtools dosfstools python
```

### Docker Setup

If you prefer an isolated containerized build environment, you can use the provided Docker workflow:

```bash
# Build the Docker image and run STAX
./docker-run.sh
```

---

## Building and Running STAX

The `makefile` provides targets for building, running, debugging, and testing:

| Target | Description |
|---|---|
| `make` | Builds bootloader, kernel, ELF binaries, `.launch` packages, and generates `os.bin` |
| `make run` | Launches STAX in QEMU with graphical framebuffer window and serial console |
| `make run-cli` | Launches STAX in headless terminal mode (`-nographic`) |
| `make clean` | Cleans object files, host tools, and build outputs in `build/` |
| `make clean-all` | Cleans all artifacts including the disk image `os.bin` |
| `make bench` | Builds and runs the automated benchmark suite in QEMU |
| `make test` | Executes the automated test suite |
| `make debug` | Starts QEMU in GDB wait state (`-s -S`) |
| `make gdb` | Connects `arm-none-eabi-gdb` to the running QEMU instance |

---

## Project Architecture & Directory Layout

```text
stax/
├── boot/           # Bootloader startup assembly, FAT16 stage-2 loader, A/B secure boot
├── kernel/         # Kernel core: scheduler, MMU paging, syscalls, processes, signals
├── drivers/        # Hardware drivers: PL181 SD, SP804 timer, PL050 mouse/kbd, PL110 LCD, VIC, PTY
├── fs/             # Virtual File System (VFS), FatFs integration, DevFS, pipes, .launch loader
├── mm/             # Physical page allocator (bitmap/buddy) and kernel heap allocator
├── ui/             # Window Manager (WM), compositing engine, event loop, desktop shell, BMP loader
├── apps/           # GUI & CLI apps: calculator, text editor, file manager, terminal, sysinfo, browser
├── games/          # Built-in games (Snake, Slime Escape, Craft) and DOOM port
├── user/           # Userspace runtime (crt0.s, ulib.c, user.ld, userspace programs)
├── crypto/         # Monocypher (Ed25519, ChaCha20), SHA-256, CRC-32
├── firmware/       # Secure firmware image formats, verification, update state machine
├── include/        # Kernel and system header files
├── net/            # lwIP network stack integration and network interface drivers
├── tools/          # Host-side development utilities (stax-sign, launch-pack)
├── bench/          # Benchmark framework and stress tests
├── docs/           # Technical documentation and architecture specifications
├── scripts/        # Asset generation and disk image utility scripts
└── makefile        # Main build automation system
```

---

## Coding Guidelines & Standards

STAX operates in a bare-metal, freestanding ARM environment. Adhering to strict coding standards ensures kernel stability, predictable memory management, and cross-platform reproducibility:

1. **Language & Dialect**:
   - Kernel and userland code is written in standard **C99** (with GNU extensions where necessary).
   - Freestanding environment: Do not rely on standard host C libraries (`stdlib.h`, `stdio.h`, `unistd.h`) in kernel or userland code. Use STAX internal headers (`include/string.h`, `include/system.h`, `user/ulib.h`, etc.).

2. **Code Style & Formatting**:
   - Indentation: Use **4 spaces** (no tabs).
   - Naming Conventions:
     - Functions & variables: `snake_case` (e.g., `vfs_open()`, `sched_yield()`).
     - Types & structs: `snake_case_t` (e.g., `pcb_t`, `wm_window_t`).
     - Constants & Macros: `UPPER_SNAKE_CASE` (e.g., `MAX_PROCESSES`, `PAGE_SIZE`).
   - Braces: K&R style (opening brace on same line for functions and control structures).
   - Include Guards: Every header file must include `#ifndef FILENAME_H` guards.

3. **Memory Safety & Bare-Metal Rules**:
   - Always check allocation return values for `NULL`.
   - Never assume infinite heap space; release dynamically allocated memory when no longer needed.
   - Avoid recursive functions with deep or unbounded recursion to protect the kernel stack.
   - Use fixed-width integer types (`uint32_t`, `int32_t`, `uint8_t`, `size_t`) from `<stdint.h>` and `<stddef.h>`.

4. **Comments & Documentation**:
   - Write clear, concise comments explaining *why* something is done, especially for hardware interactions, register bit manipulation, and assembly routines.

---

## Creating Features

### Adding a New Userland Application

1. Create your application source in `user/<app_name>.c` or `apps/app_<name>.c`.
2. Include the userland runtime library header:
   ```c
   #include "user/ulib.h"
   ```
3. Implement `main(int argc, char *argv[])`.
4. Register your application binary in `makefile` to link with `user/crt0.s`, `user/ulib.c`, and `user/user.ld`.
5. Run `make` to generate the `.elf` binary into `build/` and place it onto the `os.bin` disk image.

### Adding or Modifying Drivers

1. Place driver code in `drivers/<driver_name>.c` and corresponding header in `include/<driver_name>.h`.
2. Hardware register addresses should use constants from `include/memory_map.h`.
3. Register interrupt handlers with the Vectored Interrupt Controller (`drivers/vic.c`) if hardware IRQs are needed.
4. If exposing the device to userspace, register a DevFS node in `fs/devfs.c`.

### Adding a .launch Application Package

STAX supports standalone application bundles (`.launch`):
1. Build your ELF application binary.
2. Create a `manifest.txt` describing your package (name, version, author, executable, icon).
3. Use the host tool `tools/launch-pack/launch-pack` to pack your ELF, assets, and manifest into a `.launch` file (e.g. `tools/launch-pack/launch-pack myapp.launch manifest.txt build/myapp.elf`).

---

## Development Workflow

### 1. Fork & Branch

1. Fork the [STAX repository](https://github.com/shadcy/stax) on GitHub.
2. Clone your fork locally:
   ```bash
   git clone https://github.com/<your-username>/stax.git
   cd stax
   ```
3. Create a descriptive feature branch:
   ```bash
   git checkout -b feature/my-awesome-feature
   # or
   git checkout -b fix/mmu-page-fault-handling
   ```

### 2. Commit Messages

We encourage following the [Conventional Commits](https://www.conventionalcommits.org/) specification:

- `feat: add audio driver for pl041`
- `fix: correct cluster chain traversal in fat16 driver`
- `docs: update memory map in architecture documentation`
- `refactor: simplify wm window dirty rectangle redraws`
- `bench: add stress test for multi-task scheduler`
- `chore: update makefile build targets`

### 3. Testing & Verification

Before submitting your changes, ensure:
1. `make clean && make` completes without warnings or errors.
2. STAX boots cleanly in QEMU via `make run` and `make run-cli`.
3. Existing benchmarks and tests pass via `make bench` / `make test`.
4. No dead files or untracked build artifacts are left in the repository (`git status`).

### 4. Submitting a Pull Request

1. Push your branch to your GitHub fork:
   ```bash
   git push origin feature/my-awesome-feature
   ```
2. Open a Pull Request against the `main` branch of the upstream repository.
3. Provide a clear summary in your PR description:
   - What changes were made and why.
   - How the changes were tested (screenshots or QEMU logs are appreciated for UI/driver changes).
   - Any related issue numbers (e.g., `Closes #12`).
4. Be responsive to reviews and feedback!

---

## Reporting Bugs & Requesting Features

- **Bug Reports**: Open an issue on GitHub describing:
  - The behavior observed vs. expected behavior.
  - Steps to reproduce the bug in QEMU.
  - QEMU command/flags and version (`qemu-system-arm --version`).
  - Terminal logs or serial output.
- **Feature Requests**: Open an issue detailing the proposal, use cases, and design ideas.

Thank you for helping make STAX better! Happy hacking! 🚀
