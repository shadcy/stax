/**
 * @file    crt0.s
 * @author  shadcy
 * @brief   User-space C Runtime Startup for Standalone STAX ELF-32 Binaries.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

    .syntax unified
    .arch armv5te
    .text
    .align 4
    .global _start
    .type _start, %function

_start:
    /* Stack is initialized at USER_STACK_TOP by ELF loader */
    mov     fp, #0

    /* Clear .bss section */
    ldr     r0, =__bss_start
    ldr     r1, =__bss_end
    mov     r2, #0
1:
    cmp     r0, r1
    strlt   r2, [r0], #4
    blt     1b

    mov     r0, #0      /* argc = 0 */
    mov     r1, #0      /* argv = NULL */

    /* Call user main */
    bl      main

    /* Pass return value of main to sys_exit (r0) */
    mov     r7, #1      /* SYS_EXIT = 1 */
    svc     #0

hang:
    b       hang
