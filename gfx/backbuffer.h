#ifndef GFX_BACKBUFFER_H
#define GFX_BACKBUFFER_H

#include <stdint.h>

#define GFX_WIDTH  320
#define GFX_HEIGHT 200

extern uint8_t gfx_backbuffer[GFX_WIDTH * GFX_HEIGHT];
void gfx_present(void);

#endif
