#include "wm.h"
#include "framebuffer.h"
#include "bmp.h"
#include "font8x16.h"

void image_viewer_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    (void)cw;
    (void)ch;
    
    fb_fillrect(cx, cy, cw, ch, rgb565(30, 30, 30));
    
    if (win->path[0] != '\0') {
        /* Centered drawing could be possible if we had bmp dimensions, but we just draw it at cx, cy */
        bmp_load_and_draw(win->path, cx + 10, cy + 10);
    } else {
        draw_text(cx + 10, cy + 10, "No image loaded.", COLOR_WHITE);
    }
}
