#include "palette.h"

uint16_t gfx_palette[256];

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((b & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (r >> 3));
}

void gfx_palette_init(void) {
    /* DOOM-style dark retro palette mapping */
    /* 0: Transparent/Black */
    gfx_palette[0] = rgb565(0, 0, 0);
    
    /* Grayscales (1-15) */
    for (int i = 1; i < 16; i++) {
        uint8_t v = i * 16;
        gfx_palette[i] = rgb565(v, v, v);
    }
    
    /* Greens (16-31) for slime */
    for (int i = 16; i < 32; i++) {
        uint8_t v = (i - 16) * 16;
        gfx_palette[i] = rgb565(0, v, 0);
    }
    
    /* Magentas (32-47) for acid */
    for (int i = 32; i < 48; i++) {
        uint8_t v = (i - 32) * 16;
        gfx_palette[i] = rgb565(v, 0, v);
    }
    
    /* Cyans (48-63) for portals */
    for (int i = 48; i < 64; i++) {
        uint8_t v = (i - 48) * 16;
        gfx_palette[i] = rgb565(0, v, v);
    }
    
    /* Reds (64-79) */
    for (int i = 64; i < 80; i++) {
        uint8_t v = (i - 64) * 16;
        gfx_palette[i] = rgb565(v, 0, 0);
    }
    
    /* Yellows (80-95) */
    for (int i = 80; i < 96; i++) {
        uint8_t v = (i - 80) * 16;
        gfx_palette[i] = rgb565(v, v, 0);
    }
    
    /* Pure White */
    gfx_palette[255] = rgb565(255, 255, 255);
}
