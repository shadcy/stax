/* ============================================================================
 * TIOS — vic.h
 * Versatile Interrupt Controller (VIC) driver interface
 *
 * The VIC on the ARM VersatilePB sits at base address 0x10140000.
 * It supports 32 interrupt sources and provides:
 *   – vectored interrupts (16 slots)
 *   – per-source enable/disable
 *   – IRQ / FIQ selection
 *   – software interrupts
 * ============================================================================ */

#ifndef VIC_H
#define VIC_H

#include <stdint.h>

/* --------------------------------------------------------------------
 *  Base address
 * -------------------------------------------------------------------- */
#define VIC_BASE  0x10140000UL

/* --------------------------------------------------------------------
 *  Register map (offsets from VIC_BASE)
 * -------------------------------------------------------------------- */
#define VICIRQSTATUS    (*(volatile uint32_t *)(VIC_BASE + 0x000))
#define VICFIQSTATUS    (*(volatile uint32_t *)(VIC_BASE + 0x004))
#define VICRAWINTR      (*(volatile uint32_t *)(VIC_BASE + 0x008))
#define VICINTSELECT    (*(volatile uint32_t *)(VIC_BASE + 0x00C))
#define VICINTENABLE    (*(volatile uint32_t *)(VIC_BASE + 0x010))
#define VICINTENCLEAR   (*(volatile uint32_t *)(VIC_BASE + 0x014))
#define VICSOFTINT      (*(volatile uint32_t *)(VIC_BASE + 0x018))
#define VICSOFTINTCLEAR (*(volatile uint32_t *)(VIC_BASE + 0x01C))
#define VICPROTECTION   (*(volatile uint32_t *)(VIC_BASE + 0x020))
#define VICVECTADDR     (*(volatile uint32_t *)(VIC_BASE + 0x030))
#define VICDEFVECTADDR  (*(volatile uint32_t *)(VIC_BASE + 0x034))

/* Vectored interrupt slots 0–15 */
#define VICVECTADDRn(n) (*(volatile uint32_t *)(VIC_BASE + 0x100 + ((n) * 4)))
#define VICVECTCNTLn(n) (*(volatile uint32_t *)(VIC_BASE + 0x200 + ((n) * 4)))

/* --------------------------------------------------------------------
 *  VIC interrupt source numbers on VersatilePB
 *  (extracted from ARM DDI0224 — Versatile Application Baseboard)
 * -------------------------------------------------------------------- */
#define VIC_WDOG_INT       0   /* Watchdog timer            */
#define VIC_SW_INT         1   /* Software interrupt        */
#define VIC_COMMRx_INT     2   /* Debug comms Rx            */
#define VIC_COMMTx_INT     3   /* Debug comms Tx            */
#define VIC_TIMER0_INT     4   /* SP804 Timer 0 / Counter 0 */
#define VIC_TIMER1_INT     5   /* SP804 Timer 1 / Counter 1 */
#define VIC_TIMER2_INT     6   /* SP804 Timer 2             */
#define VIC_TIMER3_INT     7   /* SP804 Timer 3             */
#define VIC_UART0_INT      12  /* UART 0 (PL011)            */
#define VIC_UART1_INT      13  /* UART 1 (PL011)            */
#define VIC_UART2_INT      14  /* UART 2 (PL011)            */
#define VIC_UART3_INT      15  /* UART 3 (PL011)            */
#define VIC_RTC_INT        19  /* Real-time clock           */
#define VIC_SSP0_INT       20  /* SSP controller 0          */
/* ... (many more, see TRM for full list) */

/* Number of vectored slots the VIC provides */
#define VIC_NUM_SLOTS  16

/* --------------------------------------------------------------------
 *  API
 * -------------------------------------------------------------------- */

/* Initialise the VIC: disable all interrupts, clear status, set default handler */
void vic_init(void);

/* Register a C handler for a vectored interrupt slot (0–15).
 * `source` is the VIC interrupt number (e.g. VIC_TIMER0_INT).
 * `handler` is the address of the C function to jump to.
 * Returns 0 on success, -1 if no free slots. */
int vic_register_handler(int slot, uint32_t source, void (*handler)(void));

/* Enable a specific interrupt source */
static inline void vic_enable_source(uint32_t source)
{
    VICINTENABLE = (1U << source);
}

/* Disable a specific interrupt source */
static inline void vic_disable_source(uint32_t source)
{
    VICINTENCLEAR = (1U << source);
}

/* Acknowledge completion of an interrupt to the VIC.
 * Write the handler address back (or any value) to clear priority logic. */
static inline void vic_acknowledge(void)
{
    VICVECTADDR = 0;
}

#endif /* VIC_H */
