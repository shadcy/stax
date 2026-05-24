/* ============================================================================
 * TIOS — framebuffer.h
 at this point we only limited to 640 x 480 16 bit color depth
i guess we can increase this to 1280x720 16 bit color depth but obviously find need to make sys optimized for
all the nedded stuff.
 * ============================================================================ */
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
//just need to mess this values [tasks for shreyash]
#define FB_WIDTH   640u
#define FB_HEIGHT  480u
//here


/* Palette — RGB565 */
#define FB_BG          0x0841u   /* dark navy      */
#define COLOR_BLACK    0x0000u
#define COLOR_WHITE    0xFFFFu
#define COLOR_RED      0xF800u
#define COLOR_GREEN    0x07E0u
#define COLOR_BLUE     0x001Fu
#define COLOR_YELLOW   0xFFE0u
#define COLOR_CYAN     0x07FFu
#define COLOR_MAGENTA  0xF81Fu
#define COLOR_GRAY     0x8410u
#define COLOR_ORANGE   0xFD20u
#define COLOR_DARK_BG  FB_BG

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{ return (uint16_t)(((r&0xF8u)<<8)|((g&0xFCu)<<3)|(b>>3)); }

int       fb_init(void);
void      fb_clear(uint16_t col);
void      fb_putpixel(int x, int y, uint16_t col);
void      fb_fillrect(int x, int y, int w, int h, uint16_t col);
void      fb_drawline(int x0, int y0, int x1, int y1, uint16_t col);
uint16_t *fb_get_buffer(void);

#endif
