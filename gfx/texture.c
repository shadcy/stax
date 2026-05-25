#include "texture.h"
#include "backbuffer.h"

/* Fixed point step texture mapping for vertical lines */
void gfx_draw_vline_textured(int x, int y1, int y2, const uint8_t *texture, int u, int v_start, int v_step) {
    if (x < 0 || x >= GFX_WIDTH) return;
    if (y1 < 0) {
        v_start += (-y1) * v_step;
        y1 = 0;
    }
    if (y2 >= GFX_HEIGHT) y2 = GFX_HEIGHT - 1;
    if (y1 > y2) return;
    
    uint8_t *dst = gfx_backbuffer + y1 * GFX_WIDTH + x;
    int v = v_start;
    
    for (int y = y1; y <= y2; y++) {
        /* Assuming 64x64 texture (shift 6 = 64) */
        int ty = (v >> 16) & 63;
        *dst = texture[(u & 63) + (ty << 6)];
        dst += GFX_WIDTH;
        v += v_step;
    }
}
