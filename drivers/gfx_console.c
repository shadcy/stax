/* ============================================================================
 * TIOS — gfx_console.c
 * Text console using Linux kernel font_8x16 (GPL-2.0)
 * 640/8 = 80 columns,  480/16 = 30 rows.
 * Cursor: simple underline, redrawn after each putc, toggled by gfx_tick().
 * ============================================================================ */

#include "gfx_console.h"
#include "framebuffer.h"
#include "font8x16.h"

#define COLS   80   /* FB_WIDTH  / FONT8X16_WIDTH  = 640/8  */
#define ROWS   30   /* FB_HEIGHT / FONT8X16_HEIGHT = 480/16 */

static int      cur_x    = 0;
static int      enabled  = 0;
static uint16_t fg       = COLOR_WHITE;

/* blink state — toggled by gfx_tick() */
static int      cur_on   = 1;
static int      blink_n  = 0;
#define BLINK_DIV  5   /* gfx_tick calls per toggle */

/* ── helpers ─────────────────────────────────────────────────────────────── */



static void draw_glyph(int col, int row, unsigned char c, uint16_t color)
{
    int px = col * FONT8X16_WIDTH;
    int py = row * FONT8X16_HEIGHT;
    const unsigned char *g = font8x16_data[(unsigned char)c];
    uint16_t *fbuf = fb_get_buffer();
    for (int r = 0; r < FONT8X16_HEIGHT; r++) {
        unsigned char bits = g[r];
        uint16_t *dst = fbuf + (py + r) * (int)FB_WIDTH + px;
        for (int b = 0; b < FONT8X16_WIDTH; b++)
            dst[b] = (bits & (0x80u >> b)) ? color : FB_BG;
    }
}

/* ── scrollback buffer ─────────────────────────────────────────────────────── */
#define MAX_LINES 256
static char term_text[MAX_LINES][COLS];
static uint16_t term_color[MAX_LINES][COLS];
static int head_line = 0;      /* Current active line in the buffer (0 to MAX_LINES-1) */
static int view_offset = 0;    /* How many lines we scrolled up from the bottom */

static void cursor_paint(int show)
{
    if (view_offset > 0) return;
    int px = cur_x * FONT8X16_WIDTH;
    int cursor_row = (head_line < ROWS) ? head_line : (ROWS - 1);
    int py = cursor_row * FONT8X16_HEIGHT;
    
    if (show) {
        fb_fillrect(px, py, FONT8X16_WIDTH, FONT8X16_HEIGHT, COLOR_GRAY_5);
    } else {
        char ch = term_text[head_line % MAX_LINES][cur_x];
        draw_glyph(cur_x, cursor_row, ch ? ch : ' ', term_color[head_line % MAX_LINES][cur_x]);
    }
}


static void gfx_render_full(void)
{
    if (!enabled) return;
    fb_clear(FB_BG);

    int start_line;
    if (head_line < ROWS) {
        start_line = 0;
    } else {
        start_line = head_line - (ROWS - 1) - view_offset;
        if (start_line < 0) start_line = 0;
    }

    for (int r = 0; r < ROWS; r++) {
        int buf_line = start_line + r;
        if (buf_line < 0 || buf_line > head_line) continue;
        
        /* Render this line */
        for (int c = 0; c < COLS; c++) {
            char ch = term_text[buf_line % MAX_LINES][c];
            if (ch >= 32) {
                draw_glyph(c, r, ch, term_color[buf_line % MAX_LINES][c]);
            }
        }
    }
    
    /* Draw cursor if at bottom */
    if (view_offset == 0) {
        cursor_paint(cur_on);
    }
}

/* ── public API ──────────────────────────────────────────────────────────── */

int gfx_console_init(void)
{
    if (fb_init() != 0) return -1;
    cur_x = 0;
    head_line = 0;
    view_offset = 0;
    blink_n = 0; cur_on = 1;
    enabled = 1;
    fg = COLOR_WHITE;
    
    for (int i=0; i<MAX_LINES; i++) {
        for (int j=0; j<COLS; j++) {
            term_text[i][j] = 0;
            term_color[i][j] = COLOR_WHITE;
        }
    }
    gfx_render_full();
    return 0;
}

void gfx_set_color(uint16_t color) { fg = color; }

void gfx_tick(void)
{
    if (!enabled) return;
    if (++blink_n >= BLINK_DIV) {
        blink_n = 0;
        cur_on  = !cur_on;
        cursor_paint(cur_on);
    }
}

static void advance_line(void)
{
    head_line++;
    cur_x = 0;
    /* Clear the new line */
    for (int i=0; i<COLS; i++) {
        term_text[head_line % MAX_LINES][i] = 0;
        term_color[head_line % MAX_LINES][i] = fg;
    }
    if (view_offset > 0) view_offset++; /* Keep view stable if we are scrolled up */
    if (view_offset >= head_line) view_offset = head_line; /* But don't scroll past top */
    
    if (view_offset == 0) gfx_render_full();
}

void gfx_putc(char c)
{
    static int in_escape = 0;

    if (in_escape) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            in_escape = 0;
        }
        return;
    }

    if (c == '\x1b') {
        in_escape = 1;
        return;
    }

    if (view_offset > 0) {
        view_offset = 0; /* Auto-scroll to bottom on output */
        gfx_render_full();
    }

    if (!enabled) return;

    cursor_paint(0);

    if (c == '\n') {
        advance_line();
    } else if (c == '\r') {
        cur_x = 0;
    } else if (c == '\b') {
        if (cur_x > 0) {
            cur_x--;
            term_text[head_line % MAX_LINES][cur_x] = 0;
            int cursor_row = (head_line < ROWS) ? head_line : (ROWS - 1);
            draw_glyph(cur_x, cursor_row, ' ', fg);
        }
    } else if ((unsigned char)c >= 32) {
        term_text[head_line % MAX_LINES][cur_x] = c;
        term_color[head_line % MAX_LINES][cur_x] = fg;
        
        int cursor_row = (head_line < ROWS) ? head_line : (ROWS - 1);
        draw_glyph(cur_x, cursor_row, c, fg);
        
        if (++cur_x >= COLS) {
            advance_line();
        }
    }
    cur_on = 1; blink_n = 0;
    cursor_paint(1);
}

void gfx_puts(const char *s)
{
    if (!enabled) return;
    while (*s) gfx_putc(*s++);
}

void gfx_puts_color(const char *s, uint16_t color)
{
    uint16_t saved = fg; fg = color;
    gfx_puts(s);
    fg = saved;
}

void gfx_clear(void)
{
    if (!enabled) return;
    head_line = 0;
    cur_x = 0;
    view_offset = 0;
    for (int i=0; i<MAX_LINES; i++) {
        for (int j=0; j<COLS; j++) {
            term_text[i][j] = 0;
            term_color[i][j] = COLOR_WHITE;
        }
    }
    gfx_render_full();
}

void gfx_console_enable(int e) { enabled = e; }

void gfx_scroll(int lines)
{
    if (!enabled) return;
    view_offset += lines;
    if (view_offset < 0) view_offset = 0;
    
    int max_scroll = head_line;
    if (max_scroll < 0) max_scroll = 0;
    if (view_offset > max_scroll) view_offset = max_scroll;
    
    gfx_render_full();
}
