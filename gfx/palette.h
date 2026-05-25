#ifndef GFX_PALETTE_H
#define GFX_PALETTE_H

#include <stdint.h>

extern uint16_t gfx_palette[256];
extern uint16_t gfx_faded_palette[256];

void gfx_palette_init(void);
void gfx_set_fade(uint8_t level);

#endif
