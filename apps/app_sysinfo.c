/* ============================================================================
 * STAX — app_sysinfo.c
 * System Information & About Dialog (Overflow-Safe & Responsive)
 * ============================================================================ */

#include "wm.h"
#include "framebuffer.h"
#include "string.h"
#include "heap.h"
#include "console.h"
#include "font.h"
#include "icons.h"

extern volatile unsigned int tick_count;
extern uint32_t heap_get_free(void);
extern uint32_t heap_get_total(void);

static void int_to_str(int val, char *buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char temp[16];
    int i = 0;
    while (val > 0) {
        temp[i++] = (val % 10) + '0';
        val /= 10;
    }
    int j = 0;
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

void sysinfo_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    (void)win;
    if (cw < 40 || ch < 40) return;

    /* Background */
    fb_fillrect(cx, cy, cw, ch, rgb565(246, 247, 250));
    
    /* Header Card */
    if (ch >= 60) {
        fb_fill_rounded_rect(cx + 8, cy + 8, 48, 48, 6, theme_get_primary_accent());
        draw_text(cx + 14, cy + 24, "STAX", COLOR_WHITE);
        
        draw_text(cx + 66, cy + 12, "STAX OS (v2.0)", rgb565(20, 24, 32));
        draw_text(cx + 66, cy + 32, "GPOS Architecture Edition", rgb565(110, 115, 130));
    }
    
    if (ch >= 130) {
        fb_drawline(cx + 8, cy + 64, cx + cw - 8, cy + 64, rgb565(215, 218, 228));
        
        /* Stats */
        draw_text(cx + 10, cy + 72, "System Uptime:", rgb565(90, 95, 110));
        char buf[32];
        int_to_str(tick_count / 1000, buf);
        int len = strlen(buf);
        buf[len] = ' '; buf[len+1] = 's'; buf[len+2] = '\0';
        draw_text(cx + 130, cy + 72, buf, rgb565(20, 24, 32));
        
        draw_text(cx + 10, cy + 90, "Architecture:", rgb565(90, 95, 110));
        draw_text(cx + 130, cy + 90, "ARM926EJ-S", rgb565(20, 24, 32));
        
        draw_text(cx + 10, cy + 108, "Display Mode:", rgb565(90, 95, 110));
        draw_text(cx + 130, cy + 108, "1024x768 16-bit RGB", rgb565(20, 24, 32));
    }
    
    /* Memory Usage Section & Progress Bar */
    if (ch >= 180) {
        fb_drawline(cx + 8, cy + 130, cx + cw - 8, cy + 130, rgb565(215, 218, 228));
        draw_text(cx + 10, cy + 136, "Memory Allocation", rgb565(20, 24, 32));
        
        uint32_t total = heap_get_total();
        uint32_t free_mem = heap_get_free();
        uint32_t used = total - free_mem;
        
        char ubuf[16], fbuf[16];
        int_to_str(used / 1024, ubuf);
        int len = strlen(ubuf);
        ubuf[len] = ' '; ubuf[len+1] = 'K'; ubuf[len+2] = 'B'; ubuf[len+3] = '\0';
        
        int_to_str(free_mem / 1024, fbuf);
        len = strlen(fbuf);
        fbuf[len] = ' '; fbuf[len+1] = 'K'; fbuf[len+2] = 'B'; fbuf[len+3] = '\0';
        
        draw_text(cx + 10, cy + 154, "Used:", rgb565(90, 95, 110));
        draw_text(cx + 56, cy + 154, ubuf, rgb565(20, 24, 32));
        
        draw_text(cx + 140, cy + 154, "Free:", rgb565(90, 95, 110));
        draw_text(cx + 186, cy + 154, fbuf, rgb565(20, 24, 32));
        
        /* Progress Bar (Strictly clamped to window boundary) */
        int pbar_y = cy + 174;
        int pbar_max_w = cw - 20;
        int pbar_h = 10;
        
        if (pbar_max_w > 10 && pbar_y + pbar_h <= cy + ch) {
            fb_fill_rounded_rect(cx + 10, pbar_y, pbar_max_w, pbar_h, 3, rgb565(215, 218, 228));
            
            int bar_w = 0;
            if (total > 0) {
                bar_w = (used * pbar_max_w) / total;
            }
            if (bar_w > pbar_max_w) bar_w = pbar_max_w;
            if (bar_w > 0) {
                fb_fill_rounded_rect(cx + 10, pbar_y, bar_w, pbar_h, 3, theme_get_primary_accent());
            }
        }
    }
}
