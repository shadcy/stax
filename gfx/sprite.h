#ifndef GFX_SPRITE_H
#define GFX_SPRITE_H

#include <stdint.h>

void gfx_draw_sprite(int x, int y, int w, int h, const uint8_t *data);
/* Draws sprite and skips color index 0 (transparent) */
void gfx_draw_sprite_trans(int x, int y, int w, int h, const uint8_t *data);

#endif
