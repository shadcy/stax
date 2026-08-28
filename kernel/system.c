/**
 * @file    system.c
 * @author  shadcy
 * @brief   Platform reset and system reboot/shutdown for ARM VersatilePB.
 *
 * Part of the STAX Operating System.
 *
 * @license MIT
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include <stdint.h>
#include "memory_map.h"
#include "system.h"

static void system_disable_interrupts(void)
{
    uint32_t cpsr;
    /* Mask both IRQ and FIQ so no task or device callback can race the reset. */
    __asm__ volatile ("mrs %0, cpsr" : "=r"(cpsr));
    cpsr |= 0xC0u;
    __asm__ volatile ("msr cpsr_c, %0" : : "r"(cpsr) : "memory");
}

static void system_drain_write_buffer(void)
{
    uint32_t zero = 0;
    /* ARM926 has no DSB instruction; this CP15 operation drains writes. */
    __asm__ volatile ("mcr p15, 0, %0, c7, c10, 4" : : "r"(zero) : "memory");
}

void system_reboot(void)
{
    system_disable_interrupts();
    system_drain_write_buffer();

    /* 1. Try VersatilePB hardware system controller reset at 0x10000000 */
    volatile uint32_t *sysctl_lock1 = (volatile uint32_t *)0x10000020;
    volatile uint32_t *sysctl_rst1  = (volatile uint32_t *)0x10000040;
    *sysctl_lock1 = 0xA05F; /* Unlock magic */
    *sysctl_rst1  = 0x105;  /* Soft reset */

    /* 2. Try VersatilePB secondary base at 0x101E0000 */
    volatile uint32_t *sysctl_lock2 = (volatile uint32_t *)0x101E0020;
    volatile uint32_t *sysctl_rst2  = (volatile uint32_t *)0x101E0040;
    *sysctl_lock2 = 0xA05F;
    *sysctl_rst2  = 0x00010000;

    system_drain_write_buffer();

    /* 3. Instant CPU warm-restart: disable MMU & caches, reset SP, jump to bootloader (0x10000) */
    __asm__ volatile (
        "mrc p15, 0, r0, c1, c0, 0\n"
        "bic r0, r0, #0x0001\n"    /* Disable MMU (bit 0) */
        "bic r0, r0, #0x0004\n"    /* Disable D-Cache (bit 2) */
        "bic r0, r0, #0x1000\n"    /* Disable I-Cache (bit 12) */
        "mcr p15, 0, r0, c1, c0, 0\n"
        "mov r0, #0\n"
        "mcr p15, 0, r0, c7, c7, 0\n"  /* Invalidate entire I/D cache */
        "mcr p15, 0, r0, c8, c7, 0\n"  /* Invalidate entire TLB */
        "ldr sp, =0x80000\n"           /* Reset SP to bootloader stack */
        "ldr r0, =0x10000\n"           /* Bootloader entry point */
        "mov pc, r0\n"
        :
        :
        : "r0", "memory"
    );

    for (;;) {
        __asm__ volatile ("nop");
    }
}
