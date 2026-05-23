/* ============================================================================
 * TIOS — gfx_console.c
 * Text console using Linux kernel font_8x16 (GPL-2.0)
 * Each character cell: 8 px wide × 16 px tall, no scaling.
 * 640/8 = 80 columns,  480/16 = 30 rows.
 * ============================================================================ */

#include "gfx_console.h"
#include "framebuffer.h"
#include "font8x16.h"

#define COLS  (FB_WIDTH  / FONT8X16_WIDTH)   /* 80 */
#define ROWS  (FB_HEIGHT / FONT8X16_HEIGHT)  /* 30 */

static int  cur_x   = 0;
static int  cur_y   = 0;
static int  enabled = 0;

/* Active text colour — change with gfx_set_color() */
static uint16_t fg = COLOR_WHITE;

/* ── draw one glyph at pixel position (px, py) ───────────────────────────── */
static void draw_glyph(int px, int py, unsigned char c, uint16_t color)
{
    const unsigned char *glyph = font8x16_data[(unsigned char)c];
    for (int row = 0; row < FONT8X16_HEIGHT; row++) {
        unsigned char bits = glyph[row];
        uint16_t *dst = fb_get_buffer() + (py + row) * FB_WIDTH + px;
        for (int col = 0; col < FONT8X16_WIDTH; col++) {
            dst[col] = (bits & (0x80u >> col)) ? color : FB_BG;
        }
    }
}

/* ── scroll all rows up by one ───────────────────────────────────────────── */
static void scroll(void)
{
    uint16_t *fbuf = fb_get_buffer();
    int src = FONT8X16_HEIGHT * FB_WIDTH;
    int dst = 0;
    int len = (ROWS - 1) * FONT8X16_HEIGHT * FB_WIDTH;
    /* memmove-style upward copy — no overlap issue (dst < src) */
    for (int i = 0; i < len; i++) fbuf[dst + i] = fbuf[src + i];
    /* Clear last row */
    uint16_t *last = fbuf + (ROWS - 1) * FONT8X16_HEIGHT * FB_WIDTH;
    int cells = FONT8X16_HEIGHT * FB_WIDTH;
    for (int i = 0; i < cells; i++) last[i] = FB_BG;
}

/* ── public API ──────────────────────────────────────────────────────────── */

int gfx_console_init(void)
{
    if (fb_init() != 0) return -1;
    cur_x = cur_y = 0;
    enabled = 1;
    fg = COLOR_WHITE;
    return 0;
}

void gfx_set_color(uint16_t color) { fg = color; }

void gfx_putc(char c)
{
    if (!enabled) return;

    if (c == '\n') {
        cur_x = 0;
        cur_y++;
        if (cur_y >= ROWS) { scroll(); cur_y = ROWS - 1; }
        return;
    }
    if (c == '\r') { cur_x = 0; return; }
    if (c == '\b') {
        if (cur_x > 0) { cur_x--; draw_glyph(cur_x * FONT8X16_WIDTH,
                                              cur_y * FONT8X16_HEIGHT, ' ', fg); }
        return;
    }
    if ((unsigned char)c < 32) return;  /* skip control chars */

    draw_glyph(cur_x * FONT8X16_WIDTH, cur_y * FONT8X16_HEIGHT, c, fg);

    cur_x++;
    if (cur_x >= COLS) { cur_x = 0; cur_y++; }
    if (cur_y >= ROWS) { scroll(); cur_y = ROWS - 1; }
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
    fb_clear(FB_BG);
    cur_x = cur_y = 0;
}

void gfx_console_enable(int e) { enabled = e; }
