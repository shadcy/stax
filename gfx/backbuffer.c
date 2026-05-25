#include "backbuffer.h"
#include "palette.h"
#include "framebuffer.h"

uint8_t gfx_backbuffer[GFX_WIDTH * GFX_HEIGHT];

/* Fast 2x scaler from 320x200 (8-bit) to 640x480 (16-bit) */
void gfx_present(void) {
    uint16_t* vram = fb_get_buffer();
    if (!vram) return;
    
    /* 480 - (200 * 2) = 80 total empty lines -> 40 top, 40 bottom */
    int letterbox_y = 40;
    
    uint16_t *dst_row1 = vram + (letterbox_y * 640);
    uint16_t *dst_row2 = dst_row1 + 640;
    
    uint8_t *src = gfx_backbuffer;
    
    for (int y = 0; y < GFX_HEIGHT; y++) {
        for (int x = 0; x < GFX_WIDTH; x++) {
            uint16_t col = gfx_palette[*src++];
            *dst_row1++ = col;
            *dst_row1++ = col;
            *dst_row2++ = col;
            *dst_row2++ = col;
        }
        /* Skip ahead to next row pair (dst_row1 and dst_row2 just finished writing a full 640 width line) */
        dst_row1 += 640;
        dst_row2 += 640;
    }
}
