/* ============================================================================
 * STAX — timer.c
 * ARM SP804 Dual Timer driver implementation
 * ============================================================================ */

#include "timer.h"

/* ============================================================================
 * timer_init — configure Timer0 for periodic interrupts.
 *
 *  interval_us : microseconds between ticks.
 *                At 1 MHz, load value = interval_us.
 * ============================================================================ */
int timer_init(uint32_t interval_us)
{
    if (interval_us == 0 || interval_us > 0xFFFFFFFFUL)
        return -1;

    /* Disable timer during configuration */
    TIMER0_CTRL = 0;

    /* Set reload value.  At 1 MHz, count decrements once per microsecond. */
    TIMER0_LOAD = interval_us;

    /* Configure:
     *   32-bit counter  (TIMER_CTRL_32BIT)
     *   periodic mode   (TIMER_CTRL_PERIODIC)
     *   interrupt on    (TIMER_CTRL_INTEN)
     *   timer enabled   (TIMER_CTRL_ENABLE)
     */
    TIMER0_CTRL = TIMER_CTRL_32BIT
                | TIMER_CTRL_PERIODIC
                | TIMER_CTRL_INTEN
                | TIMER_CTRL_ENABLE;

    return 0;
}

/* ============================================================================
 * timer_start / timer_stop — enable or disable the running timer.
 * ============================================================================ */
void timer_start(void)
{
    TIMER0_CTRL |= TIMER_CTRL_ENABLE;
}

void timer_stop(void)
{
    TIMER0_CTRL &= ~TIMER_CTRL_ENABLE;
}
