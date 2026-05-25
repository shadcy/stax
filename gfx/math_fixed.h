#ifndef GFX_MATH_FIXED_H
#define GFX_MATH_FIXED_H

#include <stdint.h>

/* 16.16 Fixed Point Mathematics */
#define FIX_SHIFT 16
#define TO_FIX(x) ((int)((x) << FIX_SHIFT))
#define FROM_FIX(x) ((x) >> FIX_SHIFT)

static inline int fix_mul(int a, int b) {
    return (int)(((int64_t)a * (int64_t)b) >> FIX_SHIFT);
}

static inline int fix_div(int a, int b) {
    if (b == 0) return 0;
    return (int)((((int64_t)a) << FIX_SHIFT) / b);
}

extern const int fix_sin_table[360];
extern const int fix_cos_table[360];

#endif
