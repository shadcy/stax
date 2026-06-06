#include "wm.h"
#include "framebuffer.h"
#include "bmp.h"
#include "font8x16.h"

void image_viewer_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    (void)cw;
    (void)ch;
    
    fb_fillrect(cx, cy, cw, ch, rgb565(30, 30, 30));
    
    if (win->path[0] != '\0') {
        if (!win->app_data) {
            int w, h;
            win->app_data = bmp_load(win->path, &w, &h);
            if (win->app_data) {
                win->saved_width = w;
                win->saved_height = h;
            } else {
                win->app_data = (void*)1; /* Mark as failed to avoid reloading */
            }
        }
        
        if (win->app_data && win->app_data != (void*)1) {
            uint16_t *pixels = (uint16_t *)win->app_data;
            int w = win->saved_width;
            int h = win->saved_height;
            for (int y = 0; y < h; y++) {
                if (cy + 10 + y >= cy + ch) break; /* Clipping bottom */
                for (int x = 0; x < w; x++) {
                    if (cx + 10 + x >= cx + cw) break; /* Clipping right */
                    fb_putpixel(cx + 10 + x, cy + 10 + y, pixels[y * w + x]);
                }
            }
        } else {
            draw_text(cx + 10, cy + 10, "Failed to load image.", COLOR_WHITE);
        }
    } else {
        draw_text(cx + 10, cy + 10, "No image loaded.", COLOR_WHITE);
    }
}
