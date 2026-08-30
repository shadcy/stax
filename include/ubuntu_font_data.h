/* ============================================================================
 * STAX — ubuntu_font_data.h
 * Authentic Canonical Ubuntu Font Glyph Atlas Declarations
 * ============================================================================ */

#ifndef UBUNTU_FONT_DATA_H
#define UBUNTU_FONT_DATA_H

#include <stdint.h>

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t advance;
    const uint8_t *alpha;
} ubuntu_glyph_t;

extern const ubuntu_glyph_t ubuntu_font_glyphs[128];

#endif /* UBUNTU_FONT_DATA_H */
