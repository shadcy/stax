/* ============================================================================
 * TIOS — app_taskmgr.c
 * Task Manager Application
 * ============================================================================ */

#include "app_taskmgr.h"
#include "wm.h"
#include "framebuffer.h"
#include "font8x16.h"

extern window_t *window_list;



void taskmgr_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    (void)win;
    
    fb_fillrect(cx, cy, cw, ch, rgb565(240, 240, 240));
    
    /* Header */
    fb_fillrect(cx, cy, cw, 24, rgb565(200, 200, 200));
    draw_text(cx + 8, cy + 4, "Application", COLOR_BLACK);
    draw_text(cx + 200, cy + 4, "Status", COLOR_BLACK);
    fb_drawline(cx, cy + 24, cx + cw - 1, cy + 24, rgb565(128, 128, 128));
    
    int item_y = cy + 28;
    
    window_t *curr = window_list;
    while (curr) {
        if (item_y + 16 > cy + ch) break;
        
        draw_text(cx + 8, item_y, curr->title, COLOR_BLACK);
        
        const char *state_str = "Unknown";
        uint16_t state_color = COLOR_BLACK;
        
        if (curr->state == WM_STATE_ACTIVE) {
            state_str = "Running";
            state_color = rgb565(0, 150, 0);
        } else if (curr->state == WM_STATE_MINIMIZED) {
            state_str = "Minimized";
            state_color = rgb565(150, 150, 0);
        } else if (curr->state == WM_STATE_HIDDEN) {
            state_str = "Hidden";
            state_color = rgb565(150, 0, 0);
        }
        
        draw_text(cx + 200, item_y, state_str, state_color);
        
        item_y += 20;
        curr = curr->next;
    }
}
