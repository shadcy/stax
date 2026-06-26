/* ============================================================================
 * STAX — font8x16.h
 * Linux kernel 8×16 VGA font — open-source (GPL-2.0+)
 * Extracted from: linux/lib/fonts/font_8x16.c
 * 256 glyphs × 16 bytes each = 4096 bytes
 *
 * Each byte = one row of 8 pixels; bit 7 = leftmost pixel.
 * ============================================================================ */

#ifndef FONT8X16_H
#define FONT8X16_H

#define FONT8X16_WIDTH   8
#define FONT8X16_HEIGHT 16
#define FONT8X16_COUNT  256

extern const unsigned char font8x16_data[256][16];

#endif /* FONT8X16_H */
