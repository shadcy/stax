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


/* Palette — BGR565 */
/*

just for the context updating here
the scheme used for this  is bgr565
means a 16-bit pixel us split like this 
5 bits for blue
6 bits for green
5 bits for red
fun facto: greens has 1 more cause human eyes are more sensitive 
to green color
i guess!


lazy to go again and again putting random colors in the current usage
need to fix this later; the fuck up!
*/ 
#define FB_BG          0x0000u   //shadcy: i messed this up for the sake of the user.
#define COLOR_BLACK    0x0000u  
#define COLOR_WHITE    0xFFFFu
#define COLOR_RED      0x001Fu
#define COLOR_GREEN    0x8410u //gray 4  
#define COLOR_BLUE     0xF800u
#define COLOR_YELLOW   0x07FFu
#define COLOR_CYAN     0xCE79u //gray 6
#define COLOR_MAGENTA  0x07FFu //now yellow
#define COLOR_GRAY     0x8410u
#define COLOR_ORANGE   0x053Fu
#define COLOR_DARK_BG  FB_BG


// /* shitting some of the original code to fuck around the color scheme

// /* ===== Minimal Modern Monochrome Palette (BGR565) ===== */

// #define FB_BG          0x0000u   /* pure black */
// #define COLOR_BLACK    0x0000u
// #define COLOR_WHITE    0xFFFFu

// /* soft grayscale tones */
// #define COLOR_GRAY_1   0x2104u   /* very dark gray */
// #define COLOR_GRAY_2   0x4208u   /* dark gray */
// #define COLOR_GRAY_3   0x630Cu   /* medium dark */
// #define COLOR_GRAY_4   0x8410u   /* medium gray */
// #define COLOR_GRAY_5   0xAD55u   /* light gray */
// #define COLOR_GRAY_6   0xCE79u   /* very light gray */

// /* aliases for compatibility */
// #define COLOR_GRAY     COLOR_GRAY_4
// #define COLOR_DARK_BG  FB_BG

// /* optional accent colors */
// #define COLOR_ACCENT   0xCE79u   /* silver */
// #define COLOR_SUCCESS  0x8410u
// #define COLOR_WARNING  0xAD55u
// #define COLOR_ERROR    0xFFFFu



static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{ return (uint16_t)(((b&0xF8u)<<8)|((g&0xFCu)<<3)|(r>>3)); }

int       fb_init(void);
void      fb_clear(uint16_t col);
void      fb_putpixel(int x, int y, uint16_t col);
void      fb_fillrect(int x, int y, int w, int h, uint16_t col);
void      fb_drawline(int x0, int y0, int x1, int y1, uint16_t col);
uint16_t *fb_get_buffer(void);

#endif
