/* ============================================================================
 * TIOS — kernel.c
 * Phase 6b — Timer interrupt enabled (SP804 Timer0, 10 Hz)
 * ============================================================================ */

#include "irq.h"
#include "timer.h"
#include "vic.h"

#define UART0_BASE  0x101f1000UL
#define UART_DR     (*(volatile unsigned int *)(UART0_BASE + 0x000))
#define UART_FR     (*(volatile unsigned int *)(UART0_BASE + 0x018))
#define UART_FR_TXFF (1 << 5)

static void kputc(char c)
{
    if (c == '\n') {
        while (UART_FR & UART_FR_TXFF);
        UART_DR = '\r';
    }
    while (UART_FR & UART_FR_TXFF);
    UART_DR = (unsigned int)c;
}

static void kputs(const char *s)
{
    while (*s) kputc(*s++);
}

static void kput_uint(unsigned int n)
{
    char buf[12];
    int i = 0;
    if (n == 0) { kputc('0'); return; }
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) kputc(buf[--i]);
}

/* ---------------------------------------------------------------------------
 * Global state
 * --------------------------------------------------------------------------- */
static volatile unsigned int tick_count = 0;

/* ---------------------------------------------------------------------------
 * Timer ISR — called by the IRQ dispatcher every timer tick.
 * --------------------------------------------------------------------------- */
static void timer_isr(void)
{
    tick_count++;
    timer_ack();   /* clear SP804 interrupt flag */

    /* Print a brief tick marker */
    kputs("[tick ");
    kput_uint(tick_count);
    kputs("]\n");
}

/* ---------------------------------------------------------------------------
 * kernel_main
 * --------------------------------------------------------------------------- */
void kernel_main(void)
{
    /* ---- Phase 6a: IRQ subsystem ---- */
    irq_system_init();

    /* ---- Phase 6b: Timer ---- */
    /* Register timer ISR with the VIC */
    irq_register(VIC_TIMER0_INT, timer_isr);

    /* Configure Timer0 for 100 ms periodic interrupts (10 Hz) */
    timer_init(100000);   /* 100 000 us = 100 ms */

    /* Enable IRQs globally */
    irq_enable();

    kputs("========================================\n");
    kputs("  TIOS Kernel — Phase 6b\n");
    kputs("========================================\n");
    kputs("Status : running\n");
    kputs("Tasks  : 0 (scheduler not yet implemented)\n");
    kputs("IRQs   : enabled\n");
    kputs("Timer  : SP804 Timer0, 10 Hz (100 ms ticks)\n");
    kputs("Heap   : not initialised\n");
    kputs("----------------------------------------\n");
    kputs("Next steps:\n");
    kputs("  Phase 6c — add simple round-robin scheduler\n");
    kputs("  Phase 6d — add slab/bump memory allocator\n");
    kputs("  Phase 6e — add FAT filesystem driver\n");
    kputs("========================================\n");
    kputs("Kernel idle loop — waiting for timer ticks.\n\n");

    while (1) {
        __asm__ volatile ("nop");
    }
}
