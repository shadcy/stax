/* ==========================================================================
 * STAX — platform reset for the ARM VersatilePB board
 * ==========================================================================
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
    uint32_t resetctl;

    system_disable_interrupts();
    system_drain_write_buffer();

    /* Preserve unrelated system-controller bits and request a full reset. */
    resetctl = VERSATILE_SYS_RESETCTL;
    VERSATILE_SYS_RESETCTL = resetctl | VERSATILE_SYS_RESETCTL_SOFT;
    system_drain_write_buffer();

    /* QEMU/the board should reset before reaching this loop. Do not attempt a
     * partial software restart if hardware reset is unavailable: that would
     * leave stale peripheral and scheduler state behind. */
    for (;;) {
        __asm__ volatile ("nop");
    }
}
