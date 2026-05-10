.global _start
.section .text
_start:
    ldr sp, =0x28000      @ Stack for bootloader
    bl bootloader_main
hang: b hang
