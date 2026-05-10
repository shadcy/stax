#include "memory_map.h"

.global _start
.section .text
_start:
    ldr sp, =BOOTLOADER_STACK      @ Stack for bootloader
    bl bootloader_main
hang: b hang
