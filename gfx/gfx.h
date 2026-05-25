#ifndef GFX_GFX_H
#define GFX_GFX_H

#include "backbuffer.h"
#include "palette.h"
#include "renderer.h"
#include "sprite.h"
#include "texture.h"
#include "profiler.h"
#include "math_fixed.h"

static inline void gfx_init(void) {
    gfx_palette_init();
}

#endif
