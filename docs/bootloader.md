# TIOS Bare-Metal ARM Bootloader Documentation

This document describes the design, implementation, and hardware-readiness audit of the TIOS bootloader for the ARM VersatilePB platform.

## 1. Overview

The TIOS bootloader is a bare-metal First Stage Bootloader (FSBL) designed to    initialize the PL181 SD Card Controller, load the kernel from a raw sector offset on an SD card into RAM, and transfer execution to the kernel.

## 2. Boot Flow

The boot sequence follows these exact steps:

### Phase 1: Bootloader Startup (`boot_startup.s`)
1. **CPU Entry**: QEMU loads `bootloader.bin` at `0x10000` and sets PC to this address.
2. **Stack Setup**: Initializes `sp` to `0x80000`.
3. **BSS Zeroing**: Zeros out the uninitialized data section (`.bss`) using word-aligned stores for efficiency.
4. **C Entry**: Jumps to `bootloader_main()`.

### Phase 2: Hardware Initialization (`bootloader.c`)
1. **UART Initialization**: Configures UART0 (PL011) at `0x101F1000` for debug logging (115200 bps, 8N1).
2. **SD Controller Power-up**: Performs the power-ramp sequence for the PL181 MMCI at `0x10005000`.
3. **SD Card Protocol**:
    - `CMD0`: Reset card.
    - `CMD8`: Probe for SD v2 (detects High Capacity support).
    - `ACMD41`: Poll for initialization complete and check `CCS` bit (SDHC vs SDSC).
    - `CMD2`/`CMD3`: Get Card ID and Relative Card Address (RCA).
    - `CMD7`: Select card to enter Transfer State.
    - `CMD16`: Set block length to 512 bytes.

### Phase 3: Kernel Loading
1. **Sector Reading**: Reads the kernel binary from the SD card starting at **Sector 64** (`KERNEL_LBA`).
2. **Memory Destination**: Loads the data into RAM at `0x100000` (1 MB mark).
3. **Addressing**: Uses byte-addressing for SDSC cards and block-addressing for SDHC cards based on the `CCS` bit.

### Phase 4: Kernel Verification
1. **Magic Number Check**: The bootloader inspects the memory at `0x100004` (offset 4 of the loaded kernel).
2. **Validation**: Compares the 4 bytes against the string `"TIOS"`.
3. **Safety**: If the magic number does not match, the bootloader prints an error and halts execution to prevent jumping into garbage memory.

### Phase 5: Kernel Handoff
1. **Jump**: Performs an absolute branch to `0x100000`.
2. **Kernel Startup (`startup.s`)**:
    - **Header**: The kernel starts with a branch instruction followed by the `"TIOS"` magic number.
    - **Vector Table Remap**: **CRITICAL** Copies the exception vector table from `0x100000` to `0x00000000` so that interrupts (Timer, UART) can function on real hardware.
    - **Mode Stacks**: Initializes separate stacks for IRQ, FIQ, and SVC modes.
    - **Kernel Main**: Jumps to `kernel_main()`.

---

## 3. Memory Map

| Range | Description |
|---|---|
| `0x00000000 - 0x0000003F` | **Exception Vector Table** (Remapped at runtime) |
| `0x00010000 - 0x0001XXXX` | **Bootloader Code/Data** (Loaded by QEMU) |
| `0x00080000` | **Bootloader Stack Top** (Grows down) |
| `0x00100000 - 0x0010XXXX` | **Kernel Code/Data** (Loaded from SD) |
| `0x00104388 - 0x0010C388` | **Kernel Heap** (32 KB, grows up) |
| `0x0010E388` | **Kernel Stack Top** (8 KB, grows down) |

---

## 4. Real Hardware Readiness Audit

A thorough audit was performed to ensure the simulation matches physical ARM VersatilePB hardware.

### 4.1 Peripheral Verification
- **Base Addresses**: All peripheral bases (UART=0x101F1000, SD=0x10005000, VIC=0x10140000, Timer=0x101E2000) match the VersatilePB Technical Reference Manual.
- **Clocking**: The SD clock starts at ~400kHz for initialization and switches to full-speed after selection.

### 4.2 Critical Hardware Gaps Fixed
1. **Vector Remapping**: Fixed an issue where interrupts would crash on real hardware because vectors remained at 0x100000. Added a copy-to-zero routine.
2. **Stack/Heap Safety**: Corrected a linker script error where the stack and heap overlapped. They are now explicitly separated.
3. **SDHC Support**: Added logic to detect `OCR[30]` (CCS bit). This ensures the bootloader works on both old SD cards (<2GB) and modern SDHC/SDXC cards.
4. **BSS Integrity**: Ensured both bootloader and kernel zero their BSS sections before executing C code.
5. **Kernel Verification**: Added a magic number check (`"TIOS"`) to ensure the kernel is correctly loaded before execution.

### 4.3 Simulation vs Reality
- **QEMU `-kernel`**: On real hardware, the First Stage Bootloader would be loaded from ROM or an internal SRAM by the SoC's BootROM. In our setup, QEMU acts as the BootROM.
- **Memory Mapping**: Address `0x0` on VersatilePB is typically aliasable to SSRAM or Flash. Our code assumes writable memory at `0x0` for vector remapping, which is standard for this board.

---

## 5. Verification Results

| Test | Status | Result |
|---|---|---|
| Build | ✅ Pass | 0 warnings, 0 errors |
| QEMU Boot | ✅ Pass | Kernel reaches interactive prompt |
| Layout Check | ✅ Pass | No overlaps between BL, Stack, Kernel, or Heap |
| SD Protocol | ✅ Pass | CMD0 through CMD17 verified against PL181 spec |
| Kernel Verification | ✅ Pass | Magic number mismatch detected and handled |

---

## 6. How to Build & Run

1. **Build**: `make clean && make`
2. **Run**: `make qemu`
3. **Disk Image**: The `os.bin` is a 1024-sector raw image. The kernel resides at offset `0x8000` (64 sectors in).

---
*Document Version: 1.1*
*Last Updated: 2026-05-10*
