#include "profiler.h"
#include "renderer.h"
#include <stdint.h>

extern volatile unsigned int tick_count;
extern const unsigned char font8x16_data[256][16];

static unsigned int last_time = 0;
static int frames = 0;
static int current_fps = 0;

static void draw_char(int x, int y, char c, uint8_t color) {
    const unsigned char *g = font8x16_data[(unsigned char)c];
    for (int r = 0; r < 16; r++) {
        unsigned char bits = g[r];
        for (int b = 0; b < 8; b++) {
            if (bits & (0x80u >> b)) {
                gfx8_putpixel(x + b, y + r, color);
            }
        }
    }
}

static void draw_string(int x, int y, const char *s, uint8_t color) {
    while (*s) {
        draw_char(x, y, *s++, color);
        x += 8;
    }
}

static void itoa_fast(int n, char s[]) {
    int i = 0;
    if (n == 0) { s[0] = '0'; s[1] = '\0'; return; }
    while (n > 0) {
        s[i++] = n % 10 + '0';
        n /= 10;
    }
    s[i] = '\0';
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char c = s[j]; s[j] = s[k]; s[k] = c;
    }
}

void gfx_profiler_update(void) {
    frames++;
    if (tick_count - last_time >= 1000) {
        current_fps = frames;
        frames = 0;
        last_time = tick_count;
    }
}

void gfx_profiler_draw(void) {
    char buf[16];
    draw_string(2, 2, "FPS: ", 255); /* 255 is White */
    itoa_fast(current_fps, buf);
    draw_string(42, 2, buf, 255);
}
