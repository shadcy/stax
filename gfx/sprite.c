#include "sprite.h"
#include "backbuffer.h"

void gfx_draw_sprite(int x, int y, int w, int h, const uint8_t *data) {
    if (x >= GFX_WIDTH || y >= GFX_HEIGHT || x + w <= 0 || y + h <= 0) return;
    
    int sx = 0, sy = 0;
    int orig_w = w;
    
    /* Clipping */
    if (x < 0) { sx = -x; w += x; x = 0; }
    if (y < 0) { sy = -y; h += y; y = 0; }
    if (x + w > GFX_WIDTH)  w = GFX_WIDTH - x;
    if (y + h > GFX_HEIGHT) h = GFX_HEIGHT - y;
    
    for (int r = 0; r < h; r++) {
        uint8_t *dst = gfx_backbuffer + (y + r) * GFX_WIDTH + x;
        const uint8_t *src = data + (sy + r) * orig_w + sx;
        for (int c = 0; c < w; c++) {
            dst[c] = src[c];
        }
    }
}

void gfx_draw_sprite_trans(int x, int y, int w, int h, const uint8_t *data) {
    if (x >= GFX_WIDTH || y >= GFX_HEIGHT || x + w <= 0 || y + h <= 0) return;
    
    int sx = 0, sy = 0;
    int orig_w = w;
    
    /* Clipping */
    if (x < 0) { sx = -x; w += x; x = 0; }
    if (y < 0) { sy = -y; h += y; y = 0; }
    if (x + w > GFX_WIDTH)  w = GFX_WIDTH - x;
    if (y + h > GFX_HEIGHT) h = GFX_HEIGHT - y;
    
    for (int r = 0; r < h; r++) {
        uint8_t *dst = gfx_backbuffer + (y + r) * GFX_WIDTH + x;
        const uint8_t *src = data + (sy + r) * orig_w + sx;
        for (int c = 0; c < w; c++) {
            uint8_t px = src[c];
            if (px != 0) {
                dst[c] = px;
            }
        }
    }
}
