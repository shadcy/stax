#include "engine2d.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "console.h"

static int x = 100, y = 100;
static int dx = 2, dy = 2;

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

static void test_draw(void) {
    fb_clear(COLOR_BLACK);
    fb_fillrect(x, y, 20, 20, COLOR_RED);
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
    
    kputs("Exited 2D Engine Test.\n");
}
