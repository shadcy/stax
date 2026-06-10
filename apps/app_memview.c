#include "wm.h"
#include "framebuffer.h"
#include "font8x16.h"
#include "heap.h"

extern uint8_t __heap_start[];

typedef struct {
    uint32_t base_addr;
} memview_state_t;

static const char hex_chars[] = "0123456789ABCDEF";

static void byte_to_hex(uint8_t b, char *buf) {
    buf[0] = hex_chars[(b >> 4) & 0x0F];
    buf[1] = hex_chars[b & 0x0F];
}

static void addr_to_hex(uint32_t a, char *buf) {
    for (int i = 0; i < 8; i++) {
        buf[7 - i] = hex_chars[a & 0x0F];
        a >>= 4;
    }
    buf[8] = '\0';
}

void memview_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    if (!win->app_data) {
        win->app_data = kmalloc(sizeof(memview_state_t));
        if (win->app_data) {
            ((memview_state_t*)win->app_data)->base_addr = (uint32_t)__heap_start;
        }
    }
    memview_state_t *st = (memview_state_t*)win->app_data;
    if (!st) return;

    fb_fillrect(cx, cy, cw, ch, rgb565(30, 30, 30));
    
    /* Header */
    fb_fillrect(cx, cy, cw, 20, rgb565(50, 50, 80));
    draw_text(cx + 10, cy + 2, "Memory Viewer [W/S = Scroll, U/D = Page]", COLOR_WHITE);
    
    int num_lines = (ch - 30) / 16;
    uint32_t addr = st->base_addr;
    
    char buf[64];
    for (int l = 0; l < num_lines; l++) {
        int py = cy + 30 + l * 16;
        
        addr_to_hex(addr, buf);
        buf[8] = ':'; buf[9] = ' '; buf[10] = '\0';
        draw_text(cx + 10, py, buf, rgb565(150, 200, 150));
        
        for (int i = 0; i < 8; i++) {
            uint8_t *ptr = (uint8_t*)(addr + i);
            char hx[3];
            byte_to_hex(*ptr, hx);
            hx[2] = '\0';
            draw_text(cx + 100 + i * 24, py, hx, COLOR_WHITE);
            
            /* ASCII view */
            char c = *ptr;
            if (c < 32 || c > 126) c = '.';
            char cbuf[2] = {c, '\0'};
            draw_text(cx + 310 + i * 8, py, cbuf, rgb565(200, 200, 200));
        }
        
        addr += 8;
    }
}

void memview_key_event(struct window *win, char c) {
    memview_state_t *st = (memview_state_t*)win->app_data;
    if (!st) return;
    
    if (c == 'w' || c == 'W') { 
        st->base_addr -= 8;
    } else if (c == 's' || c == 'S') {
        st->base_addr += 8;
    } else if (c == 'u' || c == 'U') {
        st->base_addr -= 128;
    } else if (c == 'd' || c == 'D') {
        st->base_addr += 128;
    }
}
