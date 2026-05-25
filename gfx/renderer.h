#ifndef GFX_RENDERER_H
#define GFX_RENDERER_H

#include <stdint.h>

void gfx8_clear(uint8_t color);
void gfx8_putpixel(int x, int y, uint8_t color);
void gfx8_fillrect(int x, int y, int w, int h, uint8_t color);
void gfx8_drawline(int x0, int y0, int x1, int y1, uint8_t color);

#endif
