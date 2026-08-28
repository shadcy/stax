/*
 * @file    boot_startup.s
 * @author  shadcy
 * @brief   Initial assembly entry point and stack setup for STAX bootloader.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include "memory_map.h"

.global _start
.section .text
_start:
    ldr sp, =BOOTLOADER_STACK      @ Stack for bootloader

    /* Zero the .bss section (word-aligned stores, 4x faster than byte) */
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
1:  cmp r0, r1
    bge 2f
    str r2, [r0], #4
    b 1b
2:

    bl bootloader_main
hang: b hang
