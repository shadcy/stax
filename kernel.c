/* ============================================================================
 * TIOS — kernel.c
 * Phase 6e — FAT filesystem driver added
 * ============================================================================ */

#include "irq.h"
#include "timer.h"
#include "scheduler.h"
#include "heap.h"
#include "fat.h"
#include "vic.h"
#include "console.h"
#include "tasks.h"

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

    /* ---- Phase 6e: FAT filesystem ---- */
    fat_init();

    /* ---- Phase 6b: Timer ---- */
    irq_register(VIC_TIMER0_INT, timer_isr);
    timer_init(100000);   /* 100 ms = 10 Hz */

    irq_enable();

    kputs("========================================\n");
    kputs("  TIOS Kernel — Phase 6e\n");
    kputs("========================================\n");
    kputs("Status : running\n");
    kputs("Tasks  : 2 user tasks + idle\n");
    kputs("IRQs   : enabled\n");
    kputs("Timer  : SP804 Timer0, 10 Hz (100 ms ticks)\n");
    kputs("Heap   : 64 KB bump allocator with free list\n");
    kputs("FS     : FAT12/16 driver (test image)\n");
    kputs("----------------------------------------\n");
    kputs("Creating tasks...\n");

    create_tasks();
    kputs("Tasks created successfully\n");
    
    kputs("Scheduler active — round-robin every 100 ms.\n");
    kputs("========================================\n");

    while (1) {
        __asm__ volatile ("nop");
    }
}
