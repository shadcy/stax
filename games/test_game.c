#include "engine2d.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "console.h"
#include "font8x16.h"
#include "gfx_console.h"

static int x = 100, y = 100;
static int dx = 5, dy = 5;

static void test_init(void) {
    x = 100; y = 100;
}

static void test_update(int dt_ms) {
    (void)dt_ms;
    
    x += dx;
    y += dy;
    
    if (x < 0 || x > (int)FB_WIDTH - 20) dx = -dx;
    if (y < 0 || y > (int)FB_HEIGHT - 20) dy = -dy;
    
    if (kb_is_pressed('w')) y -= 5;
    if (kb_is_pressed('s')) y += 5;
    if (kb_is_pressed('a')) x -= 5;
    if (kb_is_pressed('d')) x += 5;
}

static void draw_text(int x_pos, int y_pos, const char *str, uint16_t color) {
    while (*str) {
        unsigned char c = *str++;
        const unsigned char *g = font8x16_data[c];
        for (int r = 0; r < FONT8X16_HEIGHT; r++) {
            unsigned char bits = g[r];
            for (int b = 0; b < FONT8X16_WIDTH; b++) {
                if (bits & (0x80u >> b)) {
                    fb_putpixel(x_pos + b, y_pos + r, color);
                }
            }
        }
        x_pos += FONT8X16_WIDTH;
    }
}

static void test_draw(void) {
    fb_clear(COLOR_BLACK);
    fb_fillrect(x, y, 20, 20, COLOR_RED);
    draw_text(10, 10, "2D Engine Test", COLOR_GREEN);
    draw_text(10, 30, "Controls: W A S D to move", COLOR_WHITE);
    draw_text(10, 50, "Press Q to exit back to shell", COLOR_YELLOW);
}

void cmd_test_game(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    kputs("Starting 2D Engine Test...\n");
    
    EngineApp app = {
        .init = test_init,
        .update = test_update,
        .draw = test_draw
    };
    engine2d_run(&app);
    
    /* Re-initialize console to clear screen and restore the shell layout */
    gfx_console_init();
    
    kputs("Exited 2D Engine Test.\n");
}
