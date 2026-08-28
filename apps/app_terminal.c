/* ============================================================================
 * STAX — app_terminal.c
 * Concurrent Graphical Terminal with Full Shell Command Execution & ANSI Colors
 * ============================================================================ */

#include "app_terminal.h"
#include "wm.h"
#include "framebuffer.h"
#include "font8x16.h"
#include "heap.h"
#include "string.h"
#include "command.h"
#include "console.h"

#define TERM_COLS       80
#define TERM_ROWS       64
#define TERM_INLEN      128
#define TERM_HIST_MAX   16

typedef struct {
    char     text[TERM_ROWS][TERM_COLS];
    uint16_t color[TERM_ROWS][TERM_COLS];
    int      head;                  /* Current write row index */
    int      cur_x;                 /* Write column on head row */
    uint16_t cur_col;               /* Current active text color */
    
    char     input[TERM_INLEN];
    int      input_pos;
    
    /* Command History */
    char     history[TERM_HIST_MAX][TERM_INLEN];
    int      history_count;
    int      history_idx;
    
    /* ANSI state machine */
    int      ansi_state;            /* 0=normal, 1=ESC, 2=bracket */
    int      ansi_param;
    
    int      blink_n;
    int      cur_on;
} terminal_state_t;

/* Advance to the next line in circular ring buffer */
static void term_advance(terminal_state_t *st) {
    st->head = (st->head + 1) % TERM_ROWS;
    st->cur_x = 0;
    for (int c = 0; c < TERM_COLS; c++) {
        st->text[st->head][c]  = 0;
        st->color[st->head][c] = st->cur_col;
    }
}

/* Output single character into terminal ring buffer */
static void term_putc(terminal_state_t *st, char c, uint16_t default_col) {
    /* Basic ANSI escape code parser for color formatting */
    if (st->ansi_state == 0) {
        if (c == 0x1B) {
            st->ansi_state = 1;
            return;
        }
    } else if (st->ansi_state == 1) {
        if (c == '[') {
            st->ansi_state = 2;
            st->ansi_param = 0;
            return;
        }
        st->ansi_state = 0;
    } else if (st->ansi_state == 2) {
        if (c >= '0' && c <= '9') {
            st->ansi_param = st->ansi_param * 10 + (c - '0');
            return;
        } else if (c == 'm') {
            if (st->ansi_param == 0) st->cur_col = COLOR_WHITE;
            else if (st->ansi_param == 31) st->cur_col = rgb565(255, 85, 85);       /* Red / Error */
            else if (st->ansi_param == 32) st->cur_col = theme_get_primary_accent();  /* Dynamic Active Theme Primary */
            else if (st->ansi_param == 33) st->cur_col = rgb565(250, 215, 90);      /* Yellow / Warning */
            else if (st->ansi_param == 34) st->cur_col = rgb565(90, 150, 255);      /* Deep Blue */
            else if (st->ansi_param == 35) st->cur_col = theme_get_secondary_accent();/* Dynamic Active Theme Secondary */
            else if (st->ansi_param == 36) st->cur_col = rgb565(100, 220, 255);     /* Cyan */
            else if (st->ansi_param == 37) st->cur_col = COLOR_WHITE;
            st->ansi_state = 0;
            return;
        } else {
            st->ansi_state = 0;
            return;
        }
    }

    uint16_t col = (st->cur_col != COLOR_WHITE) ? st->cur_col : default_col;

    if (c == '\n') {
        term_advance(st);
    } else if (c == '\r') {
        st->cur_x = 0;
    } else if (c == '\t') {
        int next_tab = (st->cur_x + 8) & ~7;
        while (st->cur_x < next_tab && st->cur_x < TERM_COLS) {
            st->text[st->head][st->cur_x]  = ' ';
            st->color[st->head][st->cur_x] = col;
            st->cur_x++;
        }
        if (st->cur_x >= TERM_COLS) term_advance(st);
    } else if (c == '\b' || c == 0x7F) {
        if (st->cur_x > 0) {
            st->cur_x--;
            st->text[st->head][st->cur_x] = 0;
        }
    } else if ((unsigned char)c >= 32) {
        if (st->cur_x >= TERM_COLS) {
            term_advance(st);
        }
        st->text[st->head][st->cur_x]  = c;
        st->color[st->head][st->cur_x] = col;
        st->cur_x++;
    }
}

static void term_puts(terminal_state_t *st, const char *s, uint16_t col) {
    while (*s) term_putc(st, *s++, col);
}

static void term_clear(terminal_state_t *st) {
    st->head = 0;
    st->cur_x = 0;
    st->cur_col = COLOR_WHITE;
    st->ansi_state = 0;
    for (int r = 0; r < TERM_ROWS; r++) {
        for (int c = 0; c < TERM_COLS; c++) {
            st->text[r][c]  = 0;
            st->color[r][c] = COLOR_WHITE;
        }
    }
}

static void term_init(terminal_state_t *st) {
    term_clear(st);
    st->input_pos = 0;
    st->input[0] = '\0';
    st->history_count = 0;
    st->history_idx = 0;
    st->blink_n = 0;
    st->cur_on = 1;

    /* STAX ASCII Banner in Smooth White -> Gray Gradient */
    term_puts(st, "  ____ _____  _  __  __   ____  _   _ _____ _     _     \n", rgb565(255, 255, 255));
    term_puts(st, " / ___|_   _|/ \\| \\ \\/ /  / ___|| | | | ____| |   | |    \n", rgb565(220, 225, 235));
    term_puts(st, " \\___ \\ | | / _ \\  \\  /   \\___ \\| |_| |  _| | |   | |    \n", rgb565(185, 190, 205));
    term_puts(st, "  ___) || |/ ___ \\ /  \\    ___) |  _  | |___| |___| |___ \n", rgb565(150, 155, 170));
    term_puts(st, " |____/ |_/_/   \\_/_/\\_\\  |____/|_| |_|_____|_____|_____|\n", rgb565(115, 120, 135));
    term_puts(st, " ---------------------------------------------------------\n", rgb565(65, 70, 85));
    term_puts(st, "  STAX OS v2.0 | ARM926EJ-S | 32MB Memory | Concurrent Shell\n", rgb565(210, 215, 230));
    term_puts(st, "  Type 'help' for commands | 'clear' to reset terminal\n\n", rgb565(130, 135, 150));
}

/* Redirection hook for console output */
static void term_console_hook(char c, void *ctx) {
    terminal_state_t *st = (terminal_state_t *)ctx;
    if (st) {
        term_putc(st, c, COLOR_WHITE);
    }
}

/* Fast glyph rendering */
static void draw_glyph(uint16_t *fbuf, int px, int py, char c, uint16_t color) {
    extern const unsigned char font8x16_data[256][16];
    const unsigned char *g = font8x16_data[(unsigned char)c];
    for (int gr = 0; gr < 16; gr++) {
        unsigned char bits = g[gr];
        for (int gb = 0; gb < 8; gb++) {
            if (bits & (0x80 >> gb)) {
                int sx = px + gb, sy = py + gr;
                if ((unsigned)sx < fb_width && (unsigned)sy < fb_height)
                    fbuf[sy * fb_width + sx] = color;
            }
        }
    }
}

void terminal_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    terminal_state_t *st = (terminal_state_t *)win->app_data;
    if (!st) {
        st = (terminal_state_t *)kmalloc(sizeof(terminal_state_t));
        if (!st) {
            fb_fillrect(cx, cy, cw, ch, rgb565(80, 0, 0));
            draw_text(cx + 8, cy + 8, "Out of Memory", COLOR_WHITE);
            return;
        }
        term_init(st);
        win->app_data = st;
    }

    uint16_t theme_pri = theme_get_primary_accent();

    /* Terminal Window Background (Dark Midnight Slate) */
    fb_fillrect(cx, cy, cw, ch, rgb565(16, 18, 24));

    /* Top Shell Info Bar */
    int top_bar_h = 22;
    fb_fillrect(cx, cy, cw, top_bar_h, rgb565(24, 27, 36));
    fb_drawline(cx, cy + top_bar_h - 1, cx + cw - 1, cy + top_bar_h - 1, rgb565(42, 48, 65));
    draw_text(cx + 10, cy + 3, "STAX Shell (tty0)", theme_pri);
    draw_text(cx + cw - 65, cy + 3, "1000Hz", rgb565(140, 150, 170));

    /* Cursor blink */
    if (++st->blink_n >= 30) {
        st->blink_n = 0;
        st->cur_on = !st->cur_on;
    }

    uint16_t *fbuf = fb_get_buffer();
    if (!fbuf) return;

    int text_area_h = ch - top_bar_h - 26;
    int max_rows = text_area_h / 16;
    int max_cols = (cw - 12) / 8;
    if (max_cols > TERM_COLS) max_cols = TERM_COLS;
    if (max_rows > TERM_ROWS) max_rows = TERM_ROWS;

    /* Render ring buffer ending at st->head */
    for (int r = 0; r < max_rows; r++) {
        int ring_row = (st->head - (max_rows - 1 - r) + TERM_ROWS * 2) % TERM_ROWS;
        int py = cy + top_bar_h + 4 + r * 16;
        for (int c = 0; c < max_cols; c++) {
            char ch_val = st->text[ring_row][c];
            if (ch_val >= 32 && ch_val <= 126) {
                draw_glyph(fbuf, cx + 8 + c * 8, py, ch_val, st->color[ring_row][c]);
            }
        }
    }

    /* Bottom Command Input Bar */
    int bar_y = cy + ch - 24;
    fb_fillrect(cx, bar_y, cw, 24, rgb565(22, 25, 35));
    fb_drawline(cx, bar_y, cx + cw - 1, bar_y, rgb565(42, 50, 70));

    /* Prompt + Input Line (Dynamic Theme Accent) */
    const char *prompt = "stax@kernel:~$ ";
    int px = cx + 8, py2 = bar_y + 4;
    for (const char *p = prompt; *p; p++) {
        draw_glyph(fbuf, px, py2, *p, theme_pri);
        px += 8;
    }

    for (int i = 0; i < st->input_pos; i++) {
        draw_glyph(fbuf, px, py2, st->input[i], COLOR_WHITE);
        px += 8;
    }

    if (st->cur_on) {
        fb_fillrect(px, py2, 8, 16, theme_pri);
    }
}

/* Key Event Handler */
void terminal_key_event(struct window *win, char c) {
    terminal_state_t *st = (terminal_state_t *)win->app_data;
    if (!st) return;

    if (c == '\r' || c == '\n') {
        st->input[st->input_pos] = '\0';

        /* Echo typed command with dynamic theme accent */
        term_puts(st, "stax@kernel:~$ ", theme_get_primary_accent());
        term_puts(st, st->input, COLOR_WHITE);
        term_putc(st, '\n', COLOR_WHITE);

        if (st->input_pos > 0) {
            /* Add to history */
            if (st->history_count < TERM_HIST_MAX) {
                strncpy(st->history[st->history_count++], st->input, TERM_INLEN);
            } else {
                for (int i = 0; i < TERM_HIST_MAX - 1; i++) {
                    strncpy(st->history[i], st->history[i + 1], TERM_INLEN);
                }
                strncpy(st->history[TERM_HIST_MAX - 1], st->input, TERM_INLEN);
            }
            st->history_idx = st->history_count;

            /* Check for clear/cls command */
            if (strcmp(st->input, "clear") == 0 || strcmp(st->input, "cls") == 0) {
                term_clear(st);
            } else {
                /* Redirect all kernel and shell output to this terminal instance */
                console_set_hook(term_console_hook, st);
                command_process(st->input);
                console_set_hook(NULL, NULL);
            }
        }
        st->input_pos = 0;
        st->input[0] = '\0';

    } else if (c == '\b' || c == 0x7F) {
        if (st->input_pos > 0) {
            st->input[--st->input_pos] = '\0';
        }
    } else if (c >= 32 && c <= 126 && st->input_pos < TERM_INLEN - 1) {
        st->input[st->input_pos++] = c;
        st->input[st->input_pos]   = '\0';
    }
}

static int g_term_counter = 0;

struct window *terminal_open_new(void) {
    int idx = g_term_counter++;
    int ox = 70 + (idx % 6) * 30;
    int oy = 48 + (idx % 6) * 30;
    char title[32];
    strcpy(title, "Terminal");
    if (idx > 0) {
        int tlen = 8;
        title[tlen++] = ' ';
        title[tlen++] = '#';
        title[tlen++] = '1' + (idx % 9);
        title[tlen] = '\0';
    }
    struct window *tw = wm_add_window(ox, oy, 560, 360, title, terminal_draw_window);
    if (tw) {
        tw->key_event = terminal_key_event;
    }
    return tw;
}

