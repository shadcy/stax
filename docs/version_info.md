# STAX Version Information

## Current Version
**Version:** Phase 6e
**Codename:** Memory Info Feature

### Recent Updates
* **Added `read` command:** A new console command to intelligently view all available RAM and optimally display user program space (Heap, Stack) dynamically calculated from the linker symbols.
* **Boot Sequence Verification:** Traditional MBR bootflow -> Bootloader -> Kernel.
* **Dynamic Memory Mapping:** Display sizes optimally formatted in Bytes, KB, and MB based on true footprint sizes instead of hardcoded strings.

### Kernel Components
* **Architecture:** ARM926EJ-S
* **Board Target:** VersatilePB
* **Scheduler:** Round-Robin
* **Timer:** 10 Hz (100ms ticks)
* **Filesystem:** FAT12/16

### Compilation & Build
* **Toolchain:** arm-none-eabi-gcc
* **Emulation:** QEMU
