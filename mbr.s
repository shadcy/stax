.global _start
.section .text
_start:
    /* QEMU loads os.bin at 0x10000. MBR is at 0x10000. */
    /* Copy Bootloader (Sectors 1-62) to 0x20000 */
    ldr r0, =0x10200
    ldr r1, =0x20000
    ldr r2, =31744        /* 62 * 512 bytes */
1:  cmp r2, #0
    beq 2f
    ldr r3, [r0], #4
    str r3, [r1], #4
    sub r2, r2, #4
    b 1b
2:
    ldr pc, =0x20000      /* Jump to Bootloader */

    .ltorg

    .space 510 - (. - _start)
    .byte 0x55, 0xaa
