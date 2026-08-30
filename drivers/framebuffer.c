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

/* Framebuffers at the end of 32MB RAM. Each 1024x768 FB needs ~1.5MB */
#define FB_BASE      0x01C00000u /* 28 MB mark */
#define FB_BACK_BASE 0x01E00000u /* 30 MB mark */

/* LCD enable | 16bpp (mode 4 = 5:6:5) | TFT | power on */
#define CTRL_VAL    ((1u<<11)|(1u<<5)|(4u<<1)|(1u<<0))

uint32_t fb_width = 1024;
uint32_t fb_height = 768;

static uint16_t * const fb_front = (uint16_t *)FB_BASE;
static uint16_t * const fb_back  = (uint16_t *)FB_BACK_BASE;
static uint16_t * fb = (uint16_t *)FB_BASE; /* Active buffer */
static int double_buffered = 0;

/* ── init ────────────────────────────────────────────────────────────────── */
int fb_init(void)
{
    /*
     * Register values for QEMU VersatilePB 1024x768:
     *   TIM0 = 0x3F1F3FFC
     *   TIM1 = 0x090B62FF
     *   TIM2 = 0x07FF1800
     */
    CLCD_TIM0   = 0x3F1F3FFCu;
    CLCD_TIM1   = 0x090B62FFu;
    CLCD_TIM2   = 0x07FF1800u;
    CLCD_TIM3   = 0x00000000u;
    CLCD_UPBASE = FB_BASE;
    CLCD_LPBASE = FB_BASE;
    CLCD_IMSC   = 0;

    fb_clear(FB_BG);
    
    CLCD_CTRL = CTRL_VAL; /* Enable the LCD controller */

    kputs("FB: PL110 Framebuffer OK\n");
    return 0;
}

void fb_set_resolution(uint32_t w, uint32_t h)
{
    fb_width = w;
    fb_height = h;
    
    if (w == 800 && h == 600) {
        /*
         * VESA 800x600 timing estimate for PL110
         * PPL = (800/16)-1 = 49. In TIM0, PPL is bits 2-7 -> 49 << 2 = 196 = 0xC4.
         * LPP = 600-1 = 599 = 0x257 (bits 0-9 in TIM1)
         * CPL = 800-1 = 799 = 0x31F (bits 16-25 in TIM2 -> 0x031F0000)
         */
        CLCD_TIM0 = 0x3F1F3FC4u; /* 0xC4 for 800 */
        CLCD_TIM1 = 0x090B6257u; /* 0x257 for 600 */
        CLCD_TIM2 = 0x031F1800u;
    } else {
        /* Default 1024x768 */
        CLCD_TIM0 = 0x3F1F3FFCu;
        CLCD_TIM1 = 0x090B62FFu;
        CLCD_TIM2 = 0x07FF1800u;
    }
    
    CLCD_TIM3   = 0x00000000u;
    CLCD_UPBASE = FB_BASE;
    CLCD_LPBASE = FB_BASE;
    CLCD_IMSC   = 0;
    
    CLCD_CTRL = 0; /* Disable briefly */
    for (volatile int i = 0; i < 10000; i++); /* tiny delay */
    CLCD_CTRL = CTRL_VAL; /* Re-enable */
    
    fb_clear(0x0000);
}

/* ── primitives ──────────────────────────────────────────────────────────── */
void fb_clear(uint16_t col)
{
    /* Write 2 pixels per 32-bit store, optimized via STMIA assembly */
    uint32_t w = ((uint32_t)col << 16) | col;
    uint32_t *dst = (uint32_t *)fb;
    int chunks = (fb_width * fb_height * 2) / 32;
    
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
    if ((unsigned)x < fb_width && (unsigned)y < fb_height)
        fb[y * fb_width + x] = col;
}

void fb_draw_hline(int x, int y, int w, uint16_t col)
{
    if (y < 0 || y >= (int)fb_height) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > (int)fb_width) w = fb_width - x;
    if (w <= 0) return;

    uint16_t *p = fb + y * fb_width + x;
    uint32_t col32 = ((uint32_t)col << 16) | col;

    if (((uintptr_t)p & 2) && w > 0) {
        *p++ = col;
        w--;
    }

    uint32_t *p32 = (uint32_t *)p;
    int dwords = w >> 1;
    while (dwords >= 8) {
        p32[0] = col32; p32[1] = col32; p32[2] = col32; p32[3] = col32;
        p32[4] = col32; p32[5] = col32; p32[6] = col32; p32[7] = col32;
        p32 += 8;
        dwords -= 8;
    }
    while (dwords > 0) {
        *p32++ = col32;
        dwords--;
    }
    if (w & 1) {
        *((uint16_t *)p32) = col;
    }
}

void fb_draw_vline(int x, int y, int h, uint16_t col)
{
    if (x < 0 || x >= (int)fb_width) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > (int)fb_height) h = fb_height - y;
    if (h <= 0) return;

    uint16_t *p = fb + y * fb_width + x;
    uint32_t stride = fb_width;
    while (h >= 4) {
        p[0] = col; p[stride] = col; p[stride * 2] = col; p[stride * 3] = col;
        p += stride * 4;
        h -= 4;
    }
    while (h > 0) {
        *p = col;
        p += stride;
        h--;
    }
}

void fb_fillrect(int x, int y, int w, int h, uint16_t col)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb_width)  w = fb_width  - x;
    if (y + h > (int)fb_height) h = fb_height - y;
    if (w <= 0 || h <= 0) return;

    uint32_t col32 = ((uint32_t)col << 16) | col;

    for (int r = 0; r < h; r++) {
        uint16_t *p = fb + (y + r) * fb_width + x;
        int count = w;

        /* Align to 32-bit boundary */
        if (((uintptr_t)p & 2) && count > 0) {
            *p++ = col;
            count--;
        }

        /* 32-bit bulk write (2 pixels per store, unrolled 8x) */
        uint32_t *p32 = (uint32_t *)p;
        int dwords = count >> 1;
        while (dwords >= 8) {
            p32[0] = col32; p32[1] = col32; p32[2] = col32; p32[3] = col32;
            p32[4] = col32; p32[5] = col32; p32[6] = col32; p32[7] = col32;
            p32 += 8;
            dwords -= 8;
        }
        while (dwords > 0) {
            *p32++ = col32;
            dwords--;
        }

        /* Trailing pixel for odd widths */
        if (count & 1) {
            *((uint16_t *)p32) = col;
        }
    }
}

void fb_fill_rounded_rect(int x, int y, int w, int h, int r, uint16_t col)
{
    if (r <= 0) {
        fb_fillrect(x, y, w, h, col);
        return;
    }
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    /* Center rectangle */
    fb_fillrect(x, y + r, w, h - 2 * r, col);

    /* Top & bottom segments with corner indentation */
    for (int dy = 0; dy < r; dy++) {
        /* Approximate circle curve: dx = r - sqrt(r^2 - (r-dy)^2) */
        int inset = (r * (r - dy)) / (r + 1);
        if (inset < 0) inset = 0;
        int line_w = w - 2 * inset;
        if (line_w > 0) {
            fb_draw_hline(x + inset, y + dy, line_w, col);
            fb_draw_hline(x + inset, y + h - 1 - dy, line_w, col);
        }
    }
}

void fb_drawline(int x0, int y0, int x1, int y1, uint16_t col)
{
    if (y0 == y1) {
        int x_min = (x0 < x1) ? x0 : x1;
        int w = (x0 < x1) ? (x1 - x0 + 1) : (x0 - x1 + 1);
        fb_draw_hline(x_min, y0, w, col);
        return;
    }
    if (x0 == x1) {
        int y_min = (y0 < y1) ? y0 : y1;
        int h = (y0 < y1) ? (y1 - y0 + 1) : (y0 - y1 + 1);
        fb_draw_vline(x0, y_min, h, col);
        return;
    }

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
        
        int chunks = (fb_width * fb_height * 2) / 32;
        
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
        if (screen_y < 0 || screen_y >= (int)fb_height) continue;
        
        for (int c = 0; c < w; c++) {
            int screen_x = x + c;
            if (screen_x < 0 || screen_x >= (int)fb_width) continue;
            
            fb[screen_y * fb_width + screen_x] = data[r * w + c];
        }
    }
}

void fb_draw_sprite_colorkey(int x, int y, int w, int h, const uint16_t *data, uint16_t colorkey)
{
    if (w <= 0 || h <= 0 || !data) return;
    
    for (int r = 0; r < h; r++) {
        int screen_y = y + r;
        if (screen_y < 0 || screen_y >= (int)fb_height) continue;
        
        for (int c = 0; c < w; c++) {
            int screen_x = x + c;
            if (screen_x < 0 || screen_x >= (int)fb_width) continue;
            
            uint16_t pixel = data[r * w + c];
            if (pixel != colorkey) {
                fb[screen_y * fb_width + screen_x] = pixel;
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
            if (screen_y >= 0 && screen_y < (int)fb_height && screen_x >= 0 && screen_x < (int)fb_width) {
                buffer[r * w + c] = fb[screen_y * fb_width + screen_x];
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
        if (screen_y < 0 || screen_y >= (int)fb_height) continue;
        
        for (int c = 0; c < w; c++) {
            int screen_x = x + c;
            if (screen_x < 0 || screen_x >= (int)fb_width) continue;
            
            fb[screen_y * fb_width + screen_x] = buffer[r * w + c];
        }
    }
}
