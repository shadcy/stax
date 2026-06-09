/* ============================================================================
 * TIOS — kernel.c
 * Phase 6e — FAT filesystem driver added + Graphical console
 * ============================================================================ */

#include "irq.h"
#include "timer.h"
#include "scheduler.h"
#include "mmu.h"
#include "page.h"
#include "heap.h"
#include "fat.h"
#include "vic.h"
#include "console.h"
#include "command.h"
#include "gfx_console.h"
#include "keyboard.h"
#include "mouse.h"
#include "wm.h"
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
    if ((tick_count % 10) == 0) {
        kb_poll();   /* drain PL050 FIFO into ring buffer every 10 ms */
        mouse_poll(); /* poll PL050 KMI1 for mouse events */
        /* Trigger round-robin scheduler on every 10ms tick */
        need_schedule = 1;
    }
}

/* ---------------------------------------------------------------------------
 * kernel_main
 * --------------------------------------------------------------------------- */
#include "framebuffer.h"

/* helper: print dynamic prompt */
void print_prompt(void) {
    char cwd[128];
    if (f_getcwd(cwd, sizeof(cwd)) == FR_OK) {
        gfx_set_color(COLOR_GREEN); kputs("\x1b[32m");
        kputs("tios:");
        gfx_set_color(COLOR_CYAN); kputs("\x1b[36m");
        kputs(cwd);
        gfx_set_color(COLOR_WHITE); kputs("\x1b[0m");
        kputs("> ");
    } else {
        gfx_set_color(COLOR_GREEN); kputs("\x1b[32m");
        kputs("tios> ");
        gfx_set_color(COLOR_WHITE); kputs("\x1b[0m");
    }
}

void kernel_main(void)
{
    /* ---- Initialize graphical console + keyboard + mouse ---- */
    gfx_console_init();  /* also initializes the framebuffer */
    kb_init();           /* enable PL050 PS/2 keyboard */
    mouse_init();        /* enable PL050 KMI1 PS/2 mouse */
    
    /* ---- Phase 6a: IRQ subsystem ---- */
    irq_system_init();

    /* ---- Phase 6c: Scheduler ---- */
    scheduler_init();

    /* ---- Phase 6d: Memory Subsystem (MMU, Paging, Heap) ---- */
    mmu_init();
    page_init();
    heap_init();
    
    /* Initialize Window Manager early to capture output */
    wm_init();
    
    /* 1. Terminal / Boot Log window on the left */
    window_t *boot_win = wm_add_window(10, 10, 300, 300, "Boot Log", gfx_console_draw_window);
    if (boot_win) {
        boot_win->state = WM_STATE_ACTIVE;
        boot_win->key_event = gfx_console_key_event;
        boot_win->mouse_click = gfx_console_mouse_click;
        boot_win->mouse_drag = gfx_console_mouse_drag;
    }

    gfx_console_init();

    /* ---- Phase 6e: FAT filesystem ---- */
    fat_init();
    wm_load_background("BG.BMP");

    /* ---- Phase 6b: Timer ---- */
    irq_register(VIC_TIMER0_INT, timer_isr);
    timer_init(1000);        /* 1 ms = 1000 Hz */

    irq_enable();

    kputs("========================================\n");
    kputs("  TIOS Kernel - Graphical Mode\n");
    kputs("========================================\n");
    kputs("Status : running\n");
    kputs("IRQs   : enabled\n");
    kputs("Timer  : SP804 Timer0, 1000 Hz (1 ms ticks)\n");
    kputs("MMU    : Enabled (32MB RAM, D/I Caches ON)\n");
    kputs("Heap   : Paging-backed block allocator\n");
    kputs("FS     : FAT12/16 driver (test image)\n");
    kputs("Display: 640x480 framebuffer (80x60 text)\n");
    kputs("----------------------------------------\n");

    /* Initialize command system */
    command_init();
    
    kputs("Type 'help' for available commands\n");
    kputs("Type 'game --doom' to play graphical DOOM\n");
    kputs("========================================\n");

    kputs("System initialized.\n");

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
        
        /* Always update and render GUI at 60 FPS */
        static unsigned int last_frame_tick = 0;
        unsigned int current_tick = tick_count;
        if (current_tick - last_frame_tick >= 16) {
            last_frame_tick = current_tick;
            gfx_tick();
            wm_update();
            wm_render();
        }

        if (c == 0) {
            /* Yield until next timer tick to prevent CPU spinning and bus saturation */
            unsigned int start_tick = tick_count;
            while (tick_count == start_tick) {
                __asm__ volatile ("nop");
            }
            continue;
        }

        /* Pass key to focused window first */
        extern int wm_dispatch_key(char c);
        if (wm_dispatch_key(c)) continue;

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

    while (1) {
        wm_update();
        wm_render();
        __asm__ volatile ("nop");
    }
}
