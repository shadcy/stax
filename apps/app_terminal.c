/* ============================================================================
 * TIOS — app_terminal.c
 * Standalone per-window terminal — compact 32-row buffer so it fits in a
 * 64 KB heap alongside file-manager, editor, and window structs.
 * ============================================================================ */

#include "app_terminal.h"
#include "wm.h"
#include "framebuffer.h"
#include "font8x16.h"
#include "heap.h"
#include "string.h"
#include "command.h"

/* ---- Keep the buffer small to fit in the 64 KB heap ---- */
#define TERM_COLS   72   /* visible columns                        */
#define TERM_ROWS   32   /* ring-buffer depth (32×72×3 = ~6.9 KB)  */
#define TERM_INLEN  64   /* input line length                       */

typedef struct {
    char     text[TERM_ROWS][TERM_COLS];
    uint16_t color[TERM_ROWS][TERM_COLS];
    int      head;          /* current write row (mod TERM_ROWS) */
    int      cur_x;         /* write column on head row          */
    char     input[TERM_INLEN];
    int      input_pos;
    int      blink_n;
    int      cur_on;
} terminal_state_t;

/* ---- helpers ---- */
static void term_advance(terminal_state_t *st) {
    st->head = (st->head + 1) % TERM_ROWS;
    st->cur_x = 0;
    for (int c = 0; c < TERM_COLS; c++) {
        st->text[st->head][c]  = 0;
        st->color[st->head][c] = COLOR_WHITE;
    }
}

static void term_putc(terminal_state_t *st, char c, uint16_t col) {
    if (c == '\n') {
        term_advance(st);
    } else if (c == '\r') {
        st->cur_x = 0;
    } else if (c == '\b') {
        if (st->cur_x > 0) { st->cur_x--; st->text[st->head][st->cur_x] = 0; }
    } else if ((unsigned char)c >= 32) {
        st->text[st->head][st->cur_x]  = c;
        st->color[st->head][st->cur_x] = col;
        if (++st->cur_x >= TERM_COLS) term_advance(st);
    }
}

static void term_puts(terminal_state_t *st, const char *s, uint16_t col) {
    while (*s) term_putc(st, *s++, col);
}

static void term_init(terminal_state_t *st) {
    st->head = 0; st->cur_x = 0; st->input_pos = 0;
    st->blink_n = 0; st->cur_on = 1;
    for (int r = 0; r < TERM_ROWS; r++)
        for (int c = 0; c < TERM_COLS; c++) {
            st->text[r][c]  = 0;
            st->color[r][c] = COLOR_WHITE;
        }
    term_puts(st, "T-OS Terminal v1.0\n", rgb565(100, 220, 100));
    term_puts(st, "Commands: ls, mkdir, cat, help, cls ...\n", rgb565(180, 180, 180));
    term_puts(st, "Ctrl+S saves editor files.\n\n", rgb565(140, 140, 160));
}

/* ---- Draw ---- */
static void draw_glyph(uint16_t *fbuf, int px, int py, char c, uint16_t color) {
    extern const unsigned char font8x16_data[256][16];
    const unsigned char *g = font8x16_data[(unsigned char)c];
    for (int gr = 0; gr < 16; gr++) {
        unsigned char bits = g[gr];
        for (int gb = 0; gb < 8; gb++) {
            if (bits & (0x80 >> gb)) {
                int sx = px + gb, sy = py + gr;
                if ((unsigned)sx < FB_WIDTH && (unsigned)sy < FB_HEIGHT)
                    fbuf[sy * FB_WIDTH + sx] = color;
            }
        }
    }
}

void terminal_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    terminal_state_t *st = (terminal_state_t *)win->app_data;
    if (!st) {
        st = (terminal_state_t *)kmalloc(sizeof(terminal_state_t));
        if (!st) {
            /* OOM — draw error message */
            fb_fillrect(cx, cy, cw, ch, rgb565(80, 0, 0));
            return;
        }
        term_init(st);
        win->app_data = st;
    }

    /* Dark background */
    fb_fillrect(cx, cy, cw, ch, rgb565(12, 12, 20));

    /* Cursor blink */
    if (++st->blink_n >= 40) { st->blink_n = 0; st->cur_on = !st->cur_on; }

    uint16_t *fbuf = fb_get_buffer();

    int text_area_h = ch - 22;          /* leave room for input bar */
    int max_rows = text_area_h / 16;
    int max_cols = cw / 8;
    if (max_cols > TERM_COLS) max_cols = TERM_COLS;
    if (max_rows > TERM_ROWS) max_rows = TERM_ROWS;

    /* Draw ring buffer: show the last max_rows rows ending at head */
    for (int r = 0; r < max_rows; r++) {
        /* row index into ring: head-(max_rows-1-r), wrapping */
        int ring_row = (st->head - (max_rows - 1 - r) + TERM_ROWS * 2) % TERM_ROWS;
        int py = cy + r * 16;
        for (int c = 0; c < max_cols; c++) {
            char ch_val = st->text[ring_row][c];
            if (ch_val >= 32) {
                draw_glyph(fbuf, cx + c * 8, py, ch_val, st->color[ring_row][c]);
            }
        }
    }

    /* Input bar */
    int bar_y = cy + ch - 22;
    fb_fillrect(cx, bar_y, cw, 22, rgb565(20, 20, 40));
    fb_drawline(cx, bar_y, cx + cw - 1, bar_y, rgb565(50, 160, 50));

    /* Draw prompt + input */
    const char *prompt = "tios:/> ";
    int px = cx + 4, py2 = bar_y + 3;
    for (const char *p = prompt; *p; p++) {
        draw_glyph(fbuf, px, py2, *p, rgb565(80, 200, 80));
        px += 8;
    }
    for (int i = 0; i < st->input_pos; i++) {
        draw_glyph(fbuf, px, py2, st->input[i], COLOR_WHITE);
        px += 8;
    }
    if (st->cur_on) fb_fillrect(px, py2, 8, 16, rgb565(80, 200, 80));
}

/* ---- Key handler ---- */
void terminal_key_event(struct window *win, char c) {
    terminal_state_t *st = (terminal_state_t *)win->app_data;
    if (!st) return;

    if (c == '\r' || c == '\n') {
        st->input[st->input_pos] = '\0';

        /* Echo command into text buffer */
        term_puts(st, "tios:/> ", rgb565(80, 200, 80));
        term_puts(st, st->input, COLOR_WHITE);
        term_putc(st, '\n', COLOR_WHITE);

        if (st->input_pos > 0) {
            /* Local built-ins */
            if (st->input[0]=='c' && st->input[1]=='l' && st->input[2]=='s' &&
                (st->input[3]=='\0' || st->input[3]==' ')) {
                term_init(st);
            } else {
                /* Route through global command_process — output goes to
                   gfx_console (shared boot log), which is acceptable until
                   a full output-redirection layer is added. */
                command_process(st->input);
                term_puts(st, "[done]\n", rgb565(100, 100, 100));
            }
        }
        st->input_pos = 0;

    } else if ((c == '\b' || c == 0x7F) && st->input_pos > 0) {
        st->input[--st->input_pos] = '\0';
    } else if (c >= 32 && c <= 126 && st->input_pos < TERM_INLEN - 1) {
        st->input[st->input_pos++] = c;
        st->input[st->input_pos]   = '\0';
    }
}
