#include "memory_map.h"

.global _start
.section .text
_start:
    /* QEMU loads os.bin at OS_BIN_LOAD_ADDR. MBR is at MBR_LOAD_ADDR. */
    /* Copy Bootloader to its execution address */
    ldr r0, =BOOTLOADER_SRC_ADDR
    ldr r1, =BOOTLOADER_EXEC_ADDR
    ldr r2, =BOOTLOADER_SIZE
1:  cmp r2, #0
    beq 2f
    ldr r3, [r0], #4
    str r3, [r1], #4
    sub r2, r2, #4
    b 1b
2:
    ldr pc, =BOOTLOADER_EXEC_ADDR      /* Jump to Bootloader */

    .ltorg

    .space 510 - (. - _start)
    .byte 0x55, 0xaa
