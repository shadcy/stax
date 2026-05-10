#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

/* ============================================================================
 * TIOS — memory_map.h
 * Centralized memory addresses and sizes to avoid magic numbers.
 * ============================================================================ */

/* OS Image Layout in Sectors (512 bytes each) */
#define SECTOR_SIZE         512
#define MBR_SECTORS         1
#define BOOTLOADER_SECTORS  62
#define KERNEL_SECTORS      256

/* Sizes in Bytes */
#define BOOTLOADER_SIZE     (BOOTLOADER_SECTORS * SECTOR_SIZE)
#define KERNEL_MAX_SIZE     (KERNEL_SECTORS * SECTOR_SIZE)

/* Disk Offsets */
#define MBR_OFFSET          0
#define BOOTLOADER_OFFSET   (MBR_SECTORS * SECTOR_SIZE)
#define KERNEL_OFFSET       ((MBR_SECTORS + BOOTLOADER_SECTORS) * SECTOR_SIZE)

/* Physical RAM Memory Map */
/* QEMU loads os.bin at 0x10000 */
#define OS_BIN_LOAD_ADDR    0x10000

/* Where parts of os.bin exist immediately after QEMU load */
#define MBR_LOAD_ADDR       OS_BIN_LOAD_ADDR
#define BOOTLOADER_SRC_ADDR (OS_BIN_LOAD_ADDR + BOOTLOADER_OFFSET)
#define KERNEL_SRC_ADDR     (OS_BIN_LOAD_ADDR + KERNEL_OFFSET)

/* Execution Addresses */
#define BOOTLOADER_EXEC_ADDR 0x20000
#define KERNEL_EXEC_ADDR     0x30000

/* Stack Addresses */
#define BOOTLOADER_STACK    0x28000

#endif /* MEMORY_MAP_H */
