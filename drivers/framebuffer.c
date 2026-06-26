/* ============================================================================
 * STAX — framebuffer.c
 * PL110 CLCD driver for QEMU VersatilePB — proven timing values
 * ============================================================================ */

#include "framebuffer.h"
#include "console.h"
#include "string.h"

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

/* Framebuffer at the end of 32MB RAM to avoid overlapping with large .bss */
#define FB_BASE      0x01E00000u /* 30 MB mark */
#define FB_BACK_BASE 0x01F00000u /* 31 MB mark */

/* LCD enable | 16bpp (mode 4 = 5:6:5) | TFT | power on */
#define CTRL_VAL    ((1u<<11)|(1u<<5)|(4u<<1)|(1u<<0))

static uint16_t * const fb_front = (uint16_t *)FB_BASE;
static uint16_t * const fb_back  = (uint16_t *)FB_BACK_BASE;
static uint16_t * fb = (uint16_t *)FB_BASE; /* Active buffer */
static int double_buffered = 0;

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
    /* Write 2 pixels per 32-bit store, optimized via STMIA assembly */
    uint32_t w = ((uint32_t)col << 16) | col;
    uint32_t *dst = (uint32_t *)fb;
    int chunks = (FB_WIDTH * FB_HEIGHT * 2) / 32; /* 19200 chunks of 32 bytes */
    
    asm volatile (
        "mov r3, %1\n"
        "mov r4, %1\n"
        "mov r5, %1\n"
        "mov r6, %1\n"
        "mov r7, %1\n"
        "mov r8, %1\n"
        "mov r9, %1\n"
        "mov r10, %1\n"
        "mov r2, %2\n"
        "1:\n"
        "stmia %0!, {r3-r10}\n"
        "subs r2, r2, #1\n"
        "bne 1b\n"
        : "+r"(dst)
        : "r"(w), "r"(chunks)
        : "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "memory"
    );
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

void fb_set_double_buffering(int enable)
{
    double_buffered = enable;
    if (enable) {
        fb = fb_back;
    } else {
        fb = fb_front;
    }
}

void fb_swap(void)
{
    if (double_buffered) {
        /* Swap backbuffer to frontbuffer using fast burst transfers */
        uint32_t *dst = (uint32_t *)fb_front;
        uint32_t *src = (uint32_t *)fb_back;
        
        /* 640 * 480 * 2 bytes = 153600 words */
        /* 153600 / 8 = 19200 chunks */
        int chunks = (FB_WIDTH * FB_HEIGHT * 2) / 32;
        
        asm volatile (
            "mov r2, %2\n"
            "1:\n"
            "ldmia %0!, {r3-r10}\n"
            "stmia %1!, {r3-r10}\n"
            "subs r2, r2, #1\n"
            "bne 1b\n"
            : "+r"(src), "+r"(dst)
            : "r"(chunks)
            : "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "memory"
        );
    }
}

void fb_draw_sprite(int x, int y, int w, int h, const uint16_t *data)
{
    if (w <= 0 || h <= 0 || !data) return;
    
    for (int r = 0; r < h; r++) {
        int screen_y = y + r;
        if (screen_y < 0 || screen_y >= (int)FB_HEIGHT) continue;
        
        for (int c = 0; c < w; c++) {
            int screen_x = x + c;
            if (screen_x < 0 || screen_x >= (int)FB_WIDTH) continue;
            
            fb[screen_y * FB_WIDTH + screen_x] = data[r * w + c];
        }
    }
}

void fb_draw_sprite_colorkey(int x, int y, int w, int h, const uint16_t *data, uint16_t colorkey)
{
    if (w <= 0 || h <= 0 || !data) return;
    
    for (int r = 0; r < h; r++) {
        int screen_y = y + r;
        if (screen_y < 0 || screen_y >= (int)FB_HEIGHT) continue;
        
        for (int c = 0; c < w; c++) {
            int screen_x = x + c;
            if (screen_x < 0 || screen_x >= (int)FB_WIDTH) continue;
            
            uint16_t pixel = data[r * w + c];
            if (pixel != colorkey) {
                fb[screen_y * FB_WIDTH + screen_x] = pixel;
            }
        }
    }
}

void fb_save_rect(int x, int y, int w, int h, uint16_t *buffer)
{
    if (w <= 0 || h <= 0 || !buffer) return;
    
    for (int r = 0; r < h; r++) {
        int screen_y = y + r;
        for (int c = 0; c < w; c++) {
            int screen_x = x + c;
            if (screen_y >= 0 && screen_y < (int)FB_HEIGHT && screen_x >= 0 && screen_x < (int)FB_WIDTH) {
                buffer[r * w + c] = fb[screen_y * FB_WIDTH + screen_x];
            } else {
                buffer[r * w + c] = 0;
            }
        }
    }
}

void fb_restore_rect(int x, int y, int w, int h, const uint16_t *buffer)
{
    if (w <= 0 || h <= 0 || !buffer) return;
    
    for (int r = 0; r < h; r++) {
        int screen_y = y + r;
        if (screen_y < 0 || screen_y >= (int)FB_HEIGHT) continue;
        
        for (int c = 0; c < w; c++) {
            int screen_x = x + c;
            if (screen_x < 0 || screen_x >= (int)FB_WIDTH) continue;
            
            fb[screen_y * FB_WIDTH + screen_x] = buffer[r * w + c];
        }
    }
}
