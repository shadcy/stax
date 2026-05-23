/* ============================================================================
 * TIOS — kernel.c
 * Phase 6e — FAT filesystem driver added + Graphical console
 * ============================================================================ */

#include "irq.h"
#include "timer.h"
#include "scheduler.h"
#include "heap.h"
#include "fat.h"
#include "vic.h"
#include "console.h"
#include "command.h"
#include "gfx_console.h"
#include "keyboard.h"

/* ---------------------------------------------------------------------------
 * Global state
 * --------------------------------------------------------------------------- */
volatile unsigned int tick_count = 0;

/* ---------------------------------------------------------------------------
 * Timer ISR — called by the IRQ dispatcher every timer tick.
 * --------------------------------------------------------------------------- */
static void timer_isr(void)
{
    tick_count++;
    timer_ack();
    kb_poll();   /* drain PL050 FIFO into ring buffer every 10 ms */
    /* Don't trigger scheduler for single-task system */
    /* need_schedule = 1; */
}

/* ---------------------------------------------------------------------------
 * kernel_main
 * --------------------------------------------------------------------------- */
void kernel_main(void)
{
    /* ---- Initialize graphical console + keyboard ---- */
    gfx_console_init();  /* also initializes the framebuffer */
    kb_init();           /* enable PL050 PS/2 keyboard */
    
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
    timer_init(10000);        /* 10 ms = 100 Hz */

    irq_enable();

    kputs("========================================\n");
    kputs("  TIOS Kernel - Graphical Mode\n");
    kputs("========================================\n");
    kputs("Status : running\n");
    kputs("IRQs   : enabled\n");
    kputs("Timer  : SP804 Timer0, 100 Hz (10 ms ticks)\n");
    kputs("Heap   : 64 KB bump allocator with free list\n");
    kputs("FS     : FAT12/16 driver (test image)\n");
    kputs("Display: 640x480 framebuffer (80x60 text)\n");
    kputs("----------------------------------------\n");

    /* Initialize command system */
    command_init();
    
    kputs("Type 'help' for available commands\n");
    kputs("Type 'doomgfx' to play graphical DOOM\n");
    kputs("========================================\n");

    kputs("tios> Interactive command interface ready\n");
    
    /* Simple command loop - minimal memory usage */
    static char input[64];
    static int input_pos = 0;
    static int show_prompt = 1;
    
    /* Main kernel loop - never return */
    while (1) {
        /* Show prompt when needed */
        if (show_prompt) {
            kputs("tios> ");
            show_prompt = 0;
        }
        
        /* Check for input character */
        char c = kgetc();
        if (c == 0) {
            /* No input — run idle delay then tick cursor blink */
            for (volatile int i = 0; i < 50000; i++) __asm__ volatile ("nop");
            gfx_tick();
            continue;
        }
        
        /* Handle backspace */
        if (c == '\b' || c == 127) {
            if (input_pos > 0) {
                input_pos--;
                kputc('\b');   /* gfx_putc handles erase + cursor redraw */
            }
        }
        /* Handle enter */
        else if (c == '\r' || c == '\n') {
            input[input_pos] = '\0';
            kputc('\n');
            
            /* Process command safely */
            if (input_pos > 0) {
                command_process(input);
            }
            
            /* Reset for next command */
            input_pos = 0;
            show_prompt = 1;
        }
        /* Handle printable characters */
        else if (c >= 32 && c <= 126 && input_pos < 63) {
            kputc(c);
            input[input_pos++] = c;
        }
    }
    
    /* Should never reach here */
    while (1) {
        __asm__ volatile ("nop");
    }
}
