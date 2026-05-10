/* ============================================================================
 * TIOS — timer.h
 * ARM SP804 Dual Timer driver interface (VersatilePB)
 *
 * Base address : 0x101E2000
 * Clock        : 1 MHz reference
 * ============================================================================ */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* --------------------------------------------------------------------
 *  Register base
 * -------------------------------------------------------------------- */
#define TIMER_BASE  0x101E2000UL

/* Timer 0 registers */
#define TIMER0_LOAD     (*(volatile uint32_t *)(TIMER_BASE + 0x00))
#define TIMER0_VALUE    (*(volatile uint32_t *)(TIMER_BASE + 0x04))
#define TIMER0_CTRL     (*(volatile uint32_t *)(TIMER_BASE + 0x08))
#define TIMER0_INTCLR   (*(volatile uint32_t *)(TIMER_BASE + 0x0C))
#define TIMER0_RIS      (*(volatile uint32_t *)(TIMER_BASE + 0x10))
#define TIMER0_MIS      (*(volatile uint32_t *)(TIMER_BASE + 0x14))
#define TIMER0_BGLOAD   (*(volatile uint32_t *)(TIMER_BASE + 0x18))

/* Timer 1 registers (offset +0x20) */
#define TIMER1_LOAD     (*(volatile uint32_t *)(TIMER_BASE + 0x20))
#define TIMER1_CTRL     (*(volatile uint32_t *)(TIMER_BASE + 0x28))
#define TIMER1_INTCLR   (*(volatile uint32_t *)(TIMER_BASE + 0x2C))

/* Control register bits */
#define TIMER_CTRL_ENABLE      (1U << 7)
#define TIMER_CTRL_PERIODIC    (1U << 6)
#define TIMER_CTRL_INTEN       (1U << 5)
#define TIMER_CTRL_32BIT       (1U << 2)
#define TIMER_CTRL_PRESCALE_1  (0U << 2)  /* actually bit 3:2, 00 = /1, 01 = /16, 10 = /256 */

/* Reference clock frequency (Hz) */
#define TIMER_CLK_HZ  1000000UL

/* --------------------------------------------------------------------
 *  API
 * -------------------------------------------------------------------- */

/* Initialise Timer0 for periodic interrupts.
 *  interval_us  : interval in microseconds (e.g. 10000 = 10 ms → 100 Hz)
 *  Returns 0 on success, -1 if interval is too large. */
int timer_init(uint32_t interval_us);

/* Start / stop the timer */
void timer_start(void);
void timer_stop(void);

/* Acknowledge (clear) the timer interrupt in the SP804 */
static inline void timer_ack(void)
{
    TIMER0_INTCLR = 1;   /* write any value to clear */
}

#endif /* TIMER_H */
