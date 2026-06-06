#include "wm.h"
#include "framebuffer.h"
#include "string.h"
#include "heap.h"
#include "console.h"
#include "font8x16.h"

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
    
    fb_fillrect(cx, cy, cw, ch, rgb565(240, 240, 240));
    
    /* Header */
    fb_fillrect(cx + 10, cy + 10, 64, 64, rgb565(0, 128, 128));
    draw_text(cx + 20, cy + 34, "T-OS", COLOR_WHITE);
    
    draw_text(cx + 90, cy + 20, "T-OS Version 1.0.0", COLOR_BLACK);
    draw_text(cx + 90, cy + 40, "Advanced Agentic Edition", COLOR_BLACK);
    
    fb_drawline(cx + 10, cy + 85, cx + cw - 10, cy + 85, rgb565(180, 180, 180));
    
    /* Stats */
    draw_text(cx + 10, cy + 100, "System Uptime:", COLOR_BLACK);
    char buf[32];
    int_to_str(tick_count / 1000, buf);
    int len = strlen(buf);
    buf[len] = ' '; buf[len+1] = 's'; buf[len+2] = '\0';
    draw_text(cx + 140, cy + 100, buf, COLOR_BLACK);
    
    draw_text(cx + 10, cy + 120, "Architecture:", COLOR_BLACK);
    draw_text(cx + 140, cy + 120, "ARM926EJ-S", COLOR_BLACK);
    
    draw_text(cx + 10, cy + 140, "Display:", COLOR_BLACK);
    draw_text(cx + 140, cy + 140, "640x480 16-bit", COLOR_BLACK);
    
    /* Memory */
    fb_drawline(cx + 10, cy + 165, cx + cw - 10, cy + 165, rgb565(180, 180, 180));
    draw_text(cx + 10, cy + 180, "Memory Usage", COLOR_BLACK);
    
    uint32_t total = heap_get_total();
    uint32_t free_mem = heap_get_free();
    uint32_t used = total - free_mem;
    
    int_to_str(used / 1024, buf);
    len = strlen(buf);
    buf[len] = 'K'; buf[len+1] = 'B'; buf[len+2] = '\0';
    draw_text(cx + 10, cy + 200, "Used:", COLOR_BLACK);
    draw_text(cx + 80, cy + 200, buf, COLOR_BLACK);
    
    int_to_str(free_mem / 1024, buf);
    len = strlen(buf);
    buf[len] = 'K'; buf[len+1] = 'B'; buf[len+2] = '\0';
    draw_text(cx + 160, cy + 200, "Free:", COLOR_BLACK);
    draw_text(cx + 220, cy + 200, buf, COLOR_BLACK);
    
    /* Progress Bar */
    fb_fillrect(cx + 10, cy + 220, cw - 20, 20, rgb565(200, 200, 200));
    int bar_w = 0;
    if (total > 0) bar_w = (used * (cw - 20)) / total;
    fb_fillrect(cx + 10, cy + 220, bar_w, 20, rgb565(0, 128, 0));
}
