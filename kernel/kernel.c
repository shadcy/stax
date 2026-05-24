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
#include "bmp.h"

/* ---------------------------------------------------------------------------
 * Global state
 * --------------------------------------------------------------------------- */
volatile unsigned int tick_count = 0;
volatile int fs_abort_flag = 0;

/* ---------------------------------------------------------------------------
 * Timer ISR — called by the IRQ dispatcher every timer tick.
 * --------------------------------------------------------------------------- */
static void timer_isr(void)
{
    tick_count++;
    timer_ack();
    kb_poll();   /* drain PL050 FIFO into ring buffer every 10 ms */
    /* Trigger round-robin scheduler on every timer tick */
    need_schedule = 1;
}

/* ---------------------------------------------------------------------------
 * kernel_main
 * --------------------------------------------------------------------------- */
/* helper: print dynamic prompt */
void print_prompt(void) {
    char cwd[128];
    if (f_getcwd(cwd, sizeof(cwd)) == FR_OK) {
        kputs("tios:");
        kputs(cwd);
        kputs("> ");
    } else {
        kputs("tios> ");
    }
}

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
    kputs("Type 'g:doom' to play graphical DOOM\n");
    kputs("========================================\n");

    kputs("Loading boot screen...\n");
    bmp_load_and_draw("KITTEN.BMP", 480, 30);

    kputs("tios> Interactive command interface ready\n");

    /* ---- Command history ---- */
#define HIST_MAX  8
#define HIST_LEN  64
    static char hist[HIST_MAX][HIST_LEN];
    static int  hist_count = 0;   /* how many entries stored   */
    static int  hist_pos   = 0;   /* navigation cursor         */

    /* ---- Input buffer ---- */
    static char input[HIST_LEN];
    static int  input_pos  = 0;
    static int  show_prompt = 1;

    /* ---- UART ESC-sequence state (for make qemu serial mode) ---- */
    static int esc_state = 0;   /* 0=normal, 1=got ESC, 2=got ESC+[ */

    /* helper: redraw current input line (overwrites previous content) */
#define REDRAW_LINE() do {                          \
    kputc('\r');                                    \
    print_prompt();                                 \
    for (int _i = 0; _i < input_pos; _i++)         \
        kputc(input[_i]);                           \
    /* pad + retreat to erase any longer old line */\
    kputs("        \b\b\b\b\b\b\b\b");             \
} while (0)

    /* Main kernel loop - never return */
    while (1) {
        if (show_prompt) {
            print_prompt();
            show_prompt = 0;
        }

        char c = kgetc();
        if (c == 0) {
            for (volatile int i = 0; i < 50000; i++) __asm__ volatile ("nop");
            gfx_tick();
            continue;
        }

        /* ---- UART arrow-key escape sequence decoder ---- */
        if (esc_state == 0 && c == '\x1b') { esc_state = 1; continue; }
        if (esc_state == 1) {
            esc_state = (c == '[') ? 2 : 0;
            continue;
        }
        if (esc_state == 2) {
            esc_state = 0;
            if      (c == 'A') c = '\x11';   /* map ESC[A → up sentinel   */
            else if (c == 'B') c = '\x12';   /* map ESC[B → down sentinel */
            else continue;
            /* fall through to arrow handling below */
        }

        /* ---- Up arrow (PS/2 sentinel 0x11 or UART-mapped) ---- */
        if (c == '\x11') {
            if (hist_count == 0) continue;
            if (hist_pos > 0) hist_pos--;
            /* copy history entry into input */
            int len = 0;
            while (hist[hist_pos][len] && len < HIST_LEN - 1) len++;
            for (int i = 0; i < len; i++) input[i] = hist[hist_pos][i];
            input[len] = '\0';
            input_pos = len;
            REDRAW_LINE();
            continue;
        }

        /* ---- Down arrow (PS/2 sentinel 0x12 or UART-mapped) ---- */
        if (c == '\x12') {
            if (hist_pos < hist_count) hist_pos++;
            if (hist_pos == hist_count) {
                /* past newest → clear line */
                input[0] = '\0'; input_pos = 0;
            } else {
                int len = 0;
                while (hist[hist_pos][len] && len < HIST_LEN - 1) len++;
                for (int i = 0; i < len; i++) input[i] = hist[hist_pos][i];
                input[len] = '\0';
                input_pos = len;
            }
            REDRAW_LINE();
            continue;
        }

        /* ---- Backspace ---- */
        if (c == '\b' || c == 127) {
            if (input_pos > 0) {
                input_pos--;
                kputc('\b');
            }
            continue;
        }

        /* ---- Enter ---- */
        if (c == '\r' || c == '\n') {
            input[input_pos] = '\0';
            kputc('\n');
            if (input_pos > 0) {
                /* save to history (avoid duplicate of last entry) */
                int dup = (hist_count > 0 &&
                           hist[hist_count > 0 ? hist_count - 1 : 0][0] != '\0');
                /* simple duplicate check */
                int is_dup = 0;
                if (hist_count > 0) {
                    int prev = hist_count - 1;
                    is_dup = 1;
                    for (int i = 0; i < HIST_LEN; i++) {
                        if (hist[prev][i] != input[i]) { is_dup = 0; break; }
                        if (hist[prev][i] == '\0') break;
                    }
                }
                (void)dup;
                if (!is_dup) {
                    if (hist_count < HIST_MAX) {
                        /* append */
                        for (int i = 0; i < HIST_LEN; i++) hist[hist_count][i] = input[i];
                        hist_count++;
                    } else {
                        /* ring: shift entries down, add at end */
                        for (int h = 0; h < HIST_MAX - 1; h++)
                            for (int i = 0; i < HIST_LEN; i++) hist[h][i] = hist[h+1][i];
                        for (int i = 0; i < HIST_LEN; i++) hist[HIST_MAX-1][i] = input[i];
                    }
                }
                hist_pos = hist_count;   /* reset navigator to "end" */
                command_process(input);
            }
            input_pos = 0;
            show_prompt = 1;
            continue;
        }

        /* ---- Printable character ---- */
        if (c >= 32 && c <= 126 && input_pos < HIST_LEN - 1) {
            /* typing resets history navigation to live edit */
            if (hist_pos != hist_count) hist_pos = hist_count;
            kputc(c);
            input[input_pos++] = c;
        }
    }

    while (1) { __asm__ volatile ("nop"); }
}
