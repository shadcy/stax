#include "palette.h"

uint16_t gfx_palette[256];
uint16_t gfx_faded_palette[256];

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
    
    /* Custom UI Colors */
    gfx_palette[253] = rgb565(0x23, 0xFE, 0x7C); /* #23fe7c */
    gfx_palette[254] = rgb565(0xEE, 0x3B, 0x48); /* #ee3b48 */
    
    /* Pure White */
    gfx_palette[255] = rgb565(255, 255, 255);
    
    gfx_set_fade(255);
}

void gfx_set_fade(uint8_t level) {
    if (level == 255) {
        for (int i = 0; i < 256; i++) {
            gfx_faded_palette[i] = gfx_palette[i];
        }
        return;
    }
    
    for (int i = 0; i < 256; i++) {
        uint16_t c = gfx_palette[i];
        /* Extract 5-6-5 BGR components */
        uint8_t b = (c >> 11) & 0x1F;
        uint8_t g = (c >> 5) & 0x3F;
        uint8_t r = c & 0x1F;
        
        b = (b * level) / 255;
        g = (g * level) / 255;
        r = (r * level) / 255;
        
        gfx_faded_palette[i] = (b << 11) | (g << 5) | r;
    }
}
