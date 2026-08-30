/* ============================================================================
 * STAX — app_taskmgr.c
 * Modern Task Manager & System Activity Monitor
 * ============================================================================ */

#include "app_taskmgr.h"
#include "wm.h"
#include "framebuffer.h"
#include "font.h"
#include "heap.h"
#include "string.h"

extern window_t *window_list;
extern uint32_t heap_get_free(void);
extern uint32_t heap_get_total(void);
extern volatile unsigned int tick_count;

static void taskmgr_mouse_click(struct window *win, int mx, int my, int button) {
    (void)win; (void)button;
    /* If user clicks "End Task" on the selected/hovered item */
    if (my >= 68) {
        int row = (my - 68) / 22;
        int idx = 0;
        window_t *curr = window_list;
        while (curr) {
            if (idx == row) {
                if (mx >= win->width - 60 && mx < win->width - 10) {
                    extern void wm_close_window(window_t *w);
                    wm_close_window(curr);
                }
                break;
            }
            idx++;
            curr = curr->next;
        }
    }
}

void taskmgr_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    win->mouse_click = taskmgr_mouse_click;

    /* Background (Light Clean Slate) */
    fb_fillrect(cx, cy, cw, ch, rgb565(246, 248, 252));
    
    /* Top Resource Usage Card */
    int card_h = 42;
    fb_fillrect(cx, cy, cw, card_h, rgb565(236, 239, 246));
    fb_drawline(cx, cy + card_h - 1, cx + cw - 1, cy + card_h - 1, rgb565(210, 215, 226));

    uint32_t total = heap_get_total();
    uint32_t free_mem = heap_get_free();
    uint32_t used = total - free_mem;

    /* Memory Meter */
    font_draw_text(cx + 10, cy + 6, "Memory Allocation", rgb565(30, 35, 50), FONT_STYLE_REGULAR);
    int pbar_x = cx + 10;
    int pbar_y = cy + 24;
    int pbar_w = cw - 20;
    int pbar_h = 10;

    fb_fill_rounded_rect(pbar_x, pbar_y, pbar_w, pbar_h, 3, rgb565(215, 218, 228));
    int fill_w = 0;
    if (total > 0) fill_w = (used * pbar_w) / total;
    if (fill_w > pbar_w) fill_w = pbar_w;
    if (fill_w > 0) {
        fb_fill_rounded_rect(pbar_x, pbar_y, fill_w, pbar_h, 3, theme_get_primary_accent());
    }

    /* Column Table Header */
    int hy = cy + card_h;
    int th_h = 22;
    fb_fillrect(cx, hy, cw, th_h, rgb565(228, 232, 240));
    fb_drawline(cx, hy + th_h - 1, cx + cw - 1, hy + th_h - 1, rgb565(205, 210, 222));

    font_draw_text(cx + 12, hy + 3, "Application / Window", rgb565(80, 85, 100), FONT_STYLE_REGULAR);
    font_draw_text(cx + (cw * 55 / 100), hy + 3, "Status", rgb565(80, 85, 100), FONT_STYLE_REGULAR);
    font_draw_text(cx + cw - 58, hy + 3, "Action", rgb565(80, 85, 100), FONT_STYLE_REGULAR);

    /* Process Rows */
    int item_y = hy + th_h;
    int row_idx = 0;
    window_t *curr = window_list;

    while (curr) {
        if (item_y + 22 > cy + ch - 18) break;

        /* Alternating row background */
        if (row_idx & 1) {
            fb_fillrect(cx, item_y, cw, 22, rgb565(240, 243, 249));
        }

        /* App Title */
        font_draw_text_clipped(cx + 14, item_y + 3, curr->title, rgb565(20, 24, 32), FONT_STYLE_REGULAR,
                               cx + 14, cy, cx + (cw * 52 / 100), cy + ch);

        /* Status Pill */
        const char *state_str = "Running";
        uint16_t state_bg = rgb565(220, 245, 230);
        uint16_t state_fg = rgb565(20, 140, 60);

        if (curr->state == WM_STATE_MINIMIZED) {
            state_str = "Minimized";
            state_bg = rgb565(255, 245, 220);
            state_fg = rgb565(180, 120, 10);
        } else if (curr->state == WM_STATE_HIDDEN) {
            state_str = "Hidden";
            state_bg = rgb565(240, 240, 245);
            state_fg = rgb565(120, 125, 140);
        }

        int sx = cx + (cw * 55 / 100);
        fb_fill_rounded_rect(sx, item_y + 2, 70, 18, 3, state_bg);
        int tw = font_get_string_width(state_str, FONT_STYLE_REGULAR);
        font_draw_text(sx + (70 - tw) / 2, item_y + 3, state_str, state_fg, FONT_STYLE_REGULAR);

        /* End Task Button Pill */
        int ax = cx + cw - 60;
        fb_fill_rounded_rect(ax, item_y + 2, 50, 18, 3, rgb565(240, 70, 70));
        font_draw_text(ax + 8, item_y + 3, "Close", COLOR_WHITE, FONT_STYLE_REGULAR);

        /* Divider line */
        fb_drawline(cx + 10, item_y + 21, cx + cw - 10, item_y + 21, rgb565(232, 235, 244));

        item_y += 22;
        row_idx++;
        curr = curr->next;
    }

    /* Bottom status summary */
    int sb_y = cy + ch - 18;
    fb_fillrect(cx, sb_y, cw, 18, rgb565(232, 236, 244));
    fb_drawline(cx, sb_y, cx + cw - 1, sb_y, rgb565(205, 210, 222));

    font_draw_text(cx + 10, sb_y + 2, "1000Hz Preemptive Scheduler  |  Real-Time", rgb565(90, 95, 115), FONT_STYLE_REGULAR);
}
