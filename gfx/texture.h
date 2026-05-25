#ifndef GFX_TEXTURE_H
#define GFX_TEXTURE_H

#include <stdint.h>

void gfx_draw_vline_textured(int x, int y1, int y2, const uint8_t *texture, int u, int v_start, int v_step);

#endif
