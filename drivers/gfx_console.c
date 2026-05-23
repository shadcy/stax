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
static int      cur_y    = 0;
static int      enabled  = 0;
static uint16_t fg       = COLOR_WHITE;

/* blink state — toggled by gfx_tick() */
static int      cur_on   = 1;
static int      blink_n  = 0;
#define BLINK_DIV  5   /* gfx_tick calls per toggle */

/* ── helpers ─────────────────────────────────────────────────────────────── */

static void cursor_paint(int show)
{
    /* 2-pixel underline at bottom of current cell */
    int px = cur_x * FONT8X16_WIDTH;
    int py = cur_y * FONT8X16_HEIGHT + FONT8X16_HEIGHT - 2;
    fb_fillrect(px, py, FONT8X16_WIDTH, 2,
                show ? COLOR_CYAN : FB_BG);
}

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

static void scroll_up(void)
{
    uint32_t *dst = (uint32_t *)fb_get_buffer();
    int line_words = (FONT8X16_HEIGHT * (int)FB_WIDTH) / 2;
    int total_words = (ROWS - 1) * line_words;
    
    /* 32-bit transfer for 2x speedup and bus efficiency */
    for (int i = 0; i < total_words; i++)
        dst[i] = dst[i + line_words];
        
    /* Clear last row using 32-bit write */
    uint32_t clear_word = ((uint32_t)FB_BG << 16) | FB_BG;
    uint32_t *last = dst + total_words;
    for (int i = 0; i < line_words; i++)
        last[i] = clear_word;
}

/* ── public API ──────────────────────────────────────────────────────────── */

int gfx_console_init(void)
{
    if (fb_init() != 0) return -1;
    cur_x = cur_y = 0;
    blink_n = 0; cur_on = 1;
    enabled = 1;
    fg = COLOR_WHITE;
    cursor_paint(1);
    return 0;
}

void gfx_set_color(uint16_t color) { fg = color; }

/* Called from the main idle loop — drives cursor blink without interrupts */
void gfx_tick(void)
{
    if (!enabled) return;
    if (++blink_n >= BLINK_DIV) {
        blink_n = 0;
        cur_on  = !cur_on;
        cursor_paint(cur_on);
    }
}

void gfx_putc(char c)
{
    if (!enabled) return;

    cursor_paint(0);   /* erase cursor before touching its cell */

    if (c == '\n') {
        cur_x = 0; cur_y++;
        if (cur_y >= ROWS) { scroll_up(); cur_y = ROWS - 1; }
    } else if (c == '\r') {
        cur_x = 0;
    } else if (c == '\b') {
        if (cur_x > 0) { cur_x--; draw_glyph(cur_x, cur_y, ' ', fg); }
    } else if ((unsigned char)c >= 32) {
        draw_glyph(cur_x, cur_y, c, fg);
        if (++cur_x >= COLS) {
            cur_x = 0; cur_y++;
            if (cur_y >= ROWS) { scroll_up(); cur_y = ROWS - 1; }
        }
    }

    cur_on = 1; blink_n = 0;   /* reset blink so cursor stays visible */
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
    cursor_paint(0);
    fb_clear(FB_BG);
    cur_x = cur_y = 0;
    cursor_paint(1);
}

void gfx_console_enable(int e) { enabled = e; }
