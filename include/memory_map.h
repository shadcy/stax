#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

/* ============================================================================
 * STAX — memory_map.h
 * Centralized memory addresses and sizes for the bare-metal boot flow.
 * ============================================================================ */

/* Disk Layout on SD Card (os.bin) */
#define SECTOR_SIZE         512
#define KERNEL_LBA          64      /* Kernel starts at sector 64 on the SD Card */
#define KERNEL_SECTORS      256     /* Maximum kernel size (128 KB) */

/* Execution Addresses */
#define BOOTLOADER_EXEC_ADDR 0x10000 /* QEMU -kernel loads bootloader here */
#define KERNEL_EXEC_ADDR    0x100000 /* 1 MB mark, safely avoiding any overlaps */

/* VersatilePB system controller. Writing the software-reset bit asks the
 * board (and QEMU's versatilepb machine) to perform a complete platform
 * reset, returning execution to the bootloader. */
#define VERSATILE_SYS_CTRL_BASE       0x101E0000UL
#define VERSATILE_SYS_RESETCTL        (*(volatile unsigned int *)(VERSATILE_SYS_CTRL_BASE + 0x040))
#define VERSATILE_SYS_RESETCTL_SOFT   0x00010000UL

/* Stack Addresses */
#define BOOTLOADER_STACK    0x80000  /* Bootloader stack grows down from 512 KB, very safe */

#endif /* MEMORY_MAP_H */
