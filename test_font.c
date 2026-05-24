#include <stdio.h>
#include <stdint.h>
#include "include/jbfont.h"

extern const font_t Unnamed_font;
// We can declare the array as extern to access it directly!
// But wait, it's static in jbfont.c.
// We can just iterate over all glyphs and check their data!
int main() {
    int count = 0;
    int max = 0;
    for (int g = 0; g < Unnamed_font.glyph_count; g++) {
        const glyph_t* gl = &Unnamed_font.glyph[g];
        for (int i = 0; i < gl->w * gl->h; i++) {
            if (gl->data[i] > max) max = gl->data[i];
            if (gl->data[i] > 0) count++;
        }
    }
    printf("Max: %d, Non-zero: %d\n", max, count);
    return 0;
}
