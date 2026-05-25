#include "renderer.h"
#include "backbuffer.h"

void gfx8_clear(uint8_t color) {
    /* 32-bit fast clear for the 8-bit backbuffer */
    uint32_t c32 = color | (color << 8) | (color << 16) | (color << 24);
    uint32_t *p = (uint32_t *)gfx_backbuffer;
    for (int i = 0; i < (GFX_WIDTH * GFX_HEIGHT) / 4; i++) {
        p[i] = c32;
    }
}

void gfx8_putpixel(int x, int y, uint8_t color) {
    if ((unsigned)x < GFX_WIDTH && (unsigned)y < GFX_HEIGHT) {
        gfx_backbuffer[y * GFX_WIDTH + x] = color;
    }
}

void gfx8_fillrect(int x, int y, int w, int h, uint8_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > GFX_WIDTH)  w = GFX_WIDTH - x;
    if (y + h > GFX_HEIGHT) h = GFX_HEIGHT - y;
    if (w <= 0 || h <= 0) return;
    
    for (int r = 0; r < h; r++) {
        uint8_t *p = gfx_backbuffer + (y + r) * GFX_WIDTH + x;
        for (int c = 0; c < w; c++) {
            p[c] = color;
        }
    }
}

void gfx8_drawline(int x0, int y0, int x1, int y1, uint8_t color) {
    int dx = x1 - x0, dy = y1 - y0;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    int sx = dx < 0 ? -1 : 1,   sy = dy < 0 ? -1 : 1;
    int err = ax - ay;
    for (;;) {
        gfx8_putpixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -ay) { err -= ay; x0 += sx; }
        if (e2 <  ax) { err += ax; y0 += sy; }
    }
}

void gfx8_draw_sprite(int x, int y, int w, int h, const uint8_t *data) {
    int start_c = 0;
    int start_r = 0;
    int draw_w = w;
    int draw_h = h;
    
    if (x < 0) { start_c = -x; draw_w += x; x = 0; }
    if (y < 0) { start_r = -y; draw_h += y; y = 0; }
    if (x + draw_w > GFX_WIDTH)  draw_w = GFX_WIDTH - x;
    if (y + draw_h > GFX_HEIGHT) draw_h = GFX_HEIGHT - y;
    if (draw_w <= 0 || draw_h <= 0) return;
    
    for (int r = 0; r < draw_h; r++) {
        uint8_t *p = gfx_backbuffer + (y + r) * GFX_WIDTH + x;
        const uint8_t *src = data + ((start_r + r) * w) + start_c;
        for (int c = 0; c < draw_w; c++) {
            uint8_t color = src[c];
            if (color != 0) {
                p[c] = color;
            }
        }
    }
}
