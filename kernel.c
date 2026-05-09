/* ============================================================================
 * TIOS — kernel.c
 * Phase 6d — Memory allocator added
 * ============================================================================ */

#include "irq.h"
#include "timer.h"
#include "scheduler.h"
#include "heap.h"
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
    timer_ack();
    need_schedule = 1;
}

/* ---------------------------------------------------------------------------
 * Example user tasks
 * --------------------------------------------------------------------------- */
static void task_a(void)
{
    kputs("[task A start]\n");
    char *buf = kmalloc(64);
    if (buf) {
        kputs("[task A allocated 64 bytes]\n");
        kfree(buf);
    }
    while (1) {
        kputs("[task A]\n");
        for (volatile int i = 0; i < 500000; i++) __asm__ volatile ("nop");
    }
}

static void task_b(void)
{
    static int count = 0;
    kputs("[task B start]\n");
    char *buf1 = kmalloc(32);
    char *buf2 = kmalloc(128);
    if (buf1 && buf2) {
        kputs("[task B allocated 32+128 bytes]\n");
        kfree(buf2);
        kfree(buf1);
    }
    while (1) {
        kputs("[task B ");
        kput_uint(count++);
        kputs("]\n");
        for (volatile int i = 0; i < 500000; i++) __asm__ volatile ("nop");
    }
}

/* ---------------------------------------------------------------------------
 * kernel_main
 * --------------------------------------------------------------------------- */
void kernel_main(void)
{
    /* ---- Phase 6a: IRQ subsystem ---- */
    irq_system_init();

    /* ---- Phase 6c: Scheduler ---- */
    scheduler_init();

    /* ---- Phase 6d: Heap ---- */
    heap_init();

    /* ---- Phase 6b: Timer ---- */
    irq_register(VIC_TIMER0_INT, timer_isr);
    timer_init(100000);   /* 100 ms = 10 Hz */

    irq_enable();

    kputs("========================================\n");
    kputs("  TIOS Kernel — Phase 6d\n");
    kputs("========================================\n");
    kputs("Status : running\n");
    kputs("Tasks  : 2 user tasks + idle\n");
    kputs("IRQs   : enabled\n");
    kputs("Timer  : SP804 Timer0, 10 Hz (100 ms ticks)\n");
    kputs("Heap   : 64 KB bump allocator with free list\n");
    kputs("----------------------------------------\n");
    kputs("Creating tasks...\n");

    int tid_a = task_create(task_a);
    kputs("Task A creation returned: ");
    kput_uint(tid_a);
    kputs("\n");
    
    int tid_b = task_create(task_b);
    kputs("Task B creation returned: ");
    kput_uint(tid_b);
    kputs("\n");
    
    kputs("Scheduler active — round-robin every 100 ms.\n");
    kputs("========================================\n");

    while (1) {
        __asm__ volatile ("nop");
    }
}
