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
    
    uint32_t *dst1 = (uint32_t *)(vram + (letterbox_y * 640));
    uint32_t *dst2 = (uint32_t *)(vram + ((letterbox_y + 1) * 640));
    
    uint8_t *src = gfx_backbuffer;
    
    for (int y = 0; y < GFX_HEIGHT; y++) {
        for (int x = 0; x < GFX_WIDTH; x += 4) {
            uint16_t c1 = gfx_faded_palette[*src++];
            uint16_t c2 = gfx_faded_palette[*src++];
            uint16_t c3 = gfx_faded_palette[*src++];
            uint16_t c4 = gfx_faded_palette[*src++];
            
            uint32_t p1 = ((uint32_t)c1 << 16) | c1;
            uint32_t p2 = ((uint32_t)c2 << 16) | c2;
            uint32_t p3 = ((uint32_t)c3 << 16) | c3;
            uint32_t p4 = ((uint32_t)c4 << 16) | c4;
            
            dst1[0] = p1; dst1[1] = p2; dst1[2] = p3; dst1[3] = p4;
            dst2[0] = p1; dst2[1] = p2; dst2[2] = p3; dst2[3] = p4;
            
            dst1 += 4;
            dst2 += 4;
        }
        dst1 += 320; /* Skip next row, as we just wrote two rows (320 uint32s = 640 pixels) */
        dst2 += 320;
    }
}
