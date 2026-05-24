/* ============================================================================
 * TIOS — framebuffer.c
 * PL110 CLCD driver for QEMU VersatilePB — proven timing values
 * ============================================================================ */

#include "framebuffer.h"
#include "console.h"

/* ── PL110 register map (VersatilePB @ 0x10120000) ──────────────────────── */
#define CLCD_BASE    0x10120000u
#define CLCD_TIM0   (*(volatile uint32_t *)(CLCD_BASE + 0x000))
#define CLCD_TIM1   (*(volatile uint32_t *)(CLCD_BASE + 0x004))
#define CLCD_TIM2   (*(volatile uint32_t *)(CLCD_BASE + 0x008))
#define CLCD_TIM3   (*(volatile uint32_t *)(CLCD_BASE + 0x00C))
#define CLCD_UPBASE (*(volatile uint32_t *)(CLCD_BASE + 0x010))
#define CLCD_LPBASE (*(volatile uint32_t *)(CLCD_BASE + 0x014))
#define CLCD_CTRL   (*(volatile uint32_t *)(CLCD_BASE + 0x018))
#define CLCD_IMSC   (*(volatile uint32_t *)(CLCD_BASE + 0x01C))

/* Framebuffer at 2 MB mark — past kernel code/stack/heap */
#define FB_BASE     0x00200000u

/* LCD enable | 16bpp (mode 4 = 5:6:5) | TFT | power on */
#define CTRL_VAL    ((1u<<11)|(1u<<5)|(4u<<1)|(1u<<0))

static uint16_t * const fb = (uint16_t *)FB_BASE;

/* ── init ────────────────────────────────────────────────────────────────── */
int fb_init(void)
{
    /*
     * Proven register values for QEMU VersatilePB 640×480:
     *   TIM0 = 0x3F1F3F9C
     *   TIM1 = 0x090B61DF  <-- 0x1DF = 479 (480 lines)
     *   TIM2 = 0x067F1800
     * Source: multiple bare-metal QEMU VersatilePB references (OSDev, QEMU tests)
     */
    CLCD_TIM0   = 0x3F1F3F9Cu;
    CLCD_TIM1   = 0x090B61DFu;
    CLCD_TIM2   = 0x067F1800u;
    CLCD_TIM3   = 0x00000000u;
    CLCD_UPBASE = FB_BASE;
    CLCD_LPBASE = FB_BASE;
    CLCD_IMSC   = 0;

    fb_clear(FB_BG);

    CLCD_CTRL = CTRL_VAL;

    kputs("FB: PL110 640x480 16bpp OK\n");
    return 0;
}

/* ── primitives ──────────────────────────────────────────────────────────── */
void fb_clear(uint16_t col)
{
    /* Write 2 pixels per 32-bit store for speed */
    uint32_t w = ((uint32_t)col << 16) | col;
    uint32_t *p = (uint32_t *)fb;
    for (int i = 0; i < (FB_WIDTH * FB_HEIGHT / 2); i++) p[i] = w;
}

void fb_putpixel(int x, int y, uint16_t col)
{
    if ((unsigned)x < FB_WIDTH && (unsigned)y < FB_HEIGHT)
        fb[y * FB_WIDTH + x] = col;
}

void fb_fillrect(int x, int y, int w, int h, uint16_t col)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)FB_WIDTH)  w = FB_WIDTH  - x;
    if (y + h > (int)FB_HEIGHT) h = FB_HEIGHT - y;
    if (w <= 0 || h <= 0) return;
    for (int r = 0; r < h; r++) {
        uint16_t *p = fb + (y + r) * FB_WIDTH + x;
        for (int c = 0; c < w; c++) p[c] = col;
    }
}

void fb_drawline(int x0, int y0, int x1, int y1, uint16_t col)
{
    int dx = x1-x0, dy = y1-y0;
    int ax = dx<0?-dx:dx, ay = dy<0?-dy:dy;
    int sx = dx<0?-1:1,   sy = dy<0?-1:1;
    int err = ax - ay;
    for (;;) {
        fb_putpixel(x0, y0, col);
        if (x0==x1 && y0==y1) break;
        int e2 = 2*err;
        if (e2 > -ay) { err -= ay; x0 += sx; }
        if (e2 <  ax) { err += ax; y0 += sy; }
    }
}

uint16_t *fb_get_buffer(void) { return fb; }
