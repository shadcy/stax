/* ============================================================================
 * STAX — app_memview.c
 * Dynamic Real-Time Memory Telemetry, Profiler & Hex Inspector
 * ============================================================================ */

#include "wm.h"
#include "framebuffer.h"
#include "font.h"
#include "heap.h"
#include "page.h"
#include "string.h"
#include "timer.h"

extern uint8_t __heap_start[];

#define MEMVIEW_SAMPLES 32

typedef struct {
    uint32_t base_addr;
    int      tab_mode;      /* 0 = Live Telemetry & Graph, 1 = Hex Inspector */
    uint8_t  history[MEMVIEW_SAMPLES];
    uint32_t last_sample_tick;
    int      sample_count;
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

static void num_to_str(uint32_t n, char *buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[12]; int i = 0;
    while (n) { tmp[i++] = '0' + (n % 10); n /= 10; }
    int j = 0; while (i > 0) buf[j++] = tmp[--i]; buf[j] = '\0';
}

/* Format KB into string e.g. "4.8 MB" or "840 KB" */
static void format_kb(uint32_t kb, char *out) {
    if (kb >= 1024) {
        uint32_t mb = kb / 1024;
        uint32_t dec = ((kb % 1024) * 10) / 1024;
        char num[12]; num_to_str(mb, num);
        strcpy(out, num);
        strcat(out, ".");
        char dstr[2]; dstr[0] = '0' + dec; dstr[1] = '\0';
        strcat(out, dstr);
        strcat(out, " MB");
    } else {
        char num[12]; num_to_str(kb, num);
        strcpy(out, num);
        strcat(out, " KB");
    }
}

void memview_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    if (!win->app_data) {
        memview_state_t *st = (memview_state_t *)kmalloc(sizeof(memview_state_t));
        if (!st) return;
        st->base_addr = (uint32_t)__heap_start;
        st->tab_mode = 0;
        st->last_sample_tick = 0;
        st->sample_count = MEMVIEW_SAMPLES;
        for (int i = 0; i < MEMVIEW_SAMPLES; i++) st->history[i] = 12 + (i % 6);
        win->app_data = st;
    }
    memview_state_t *st = (memview_state_t *)win->app_data;
    if (!st) return;

    /* Sample real-time memory usage */
    extern volatile unsigned int tick_count;
    uint32_t cur_ticks = tick_count;
    if (cur_ticks - st->last_sample_tick >= 500) {
        st->last_sample_tick = cur_ticks;
        uint32_t tot = get_total_memory();
        uint32_t f = get_free_memory();
        uint32_t used_kb = (tot >= f) ? (tot - f) / 1024 : 0;
        uint32_t tot_kb = tot / 1024;
        uint8_t pct = (tot_kb > 0) ? (uint8_t)(((uint64_t)used_kb * 100) / tot_kb) : 0;
        if (pct == 0 && used_kb > 0) pct = 4;
        for (int i = 0; i < MEMVIEW_SAMPLES - 1; i++) st->history[i] = st->history[i + 1];
        st->history[MEMVIEW_SAMPLES - 1] = pct;
    }

    /* Window Background */
    fb_fillrect(cx, cy, cw, ch, rgb565(26, 28, 36));

    /* Top Navigation Tabs Header */
    fb_fillrect(cx, cy, cw, 32, rgb565(34, 36, 46));
    fb_drawline(cx, cy + 31, cx + cw - 1, cy + 31, rgb565(52, 56, 70));

    /* Tab 0: Telemetry */
    uint16_t tab0_bg = (st->tab_mode == 0) ? theme_get_primary_accent() : rgb565(44, 48, 62);
    fb_fill_rounded_rect(cx + 8, cy + 5, 120, 22, 3, tab0_bg);
    font_draw_text(cx + 18, cy + 8, "Telemetry & Chart", COLOR_WHITE, FONT_STYLE_REGULAR);

    /* Tab 1: Hex Inspector */
    uint16_t tab1_bg = (st->tab_mode == 1) ? theme_get_primary_accent() : rgb565(44, 48, 62);
    fb_fill_rounded_rect(cx + 134, cy + 5, 105, 22, 3, tab1_bg);
    font_draw_text(cx + 144, cy + 8, "Hex Inspector", COLOR_WHITE, FONT_STYLE_REGULAR);

    if (st->tab_mode == 0) {
        /* ================= 1. LIVE TELEMETRY & CHART ================= */
        uint32_t tot = get_total_memory();
        uint32_t f = get_free_memory();
        uint32_t tot_kb = tot / 1024;
        uint32_t free_kb = f / 1024;
        uint32_t used_kb = (tot >= f) ? (tot - f) / 1024 : 0;
        int pct = (tot_kb > 0) ? (int)(((uint64_t)used_kb * 100) / tot_kb) : 0;

        /* Top Status Cards */
        int card_w = (cw - 32) / 3;
        if (card_w < 100) card_w = 100;
        int card_y = cy + 40;

        /* Card 1: Used Memory */
        fb_fill_rounded_rect(cx + 8, card_y, card_w, 52, 4, rgb565(36, 40, 54));
        font_draw_text(cx + 16, card_y + 6, "USED MEMORY", rgb565(140, 145, 165), FONT_STYLE_REGULAR);
        char used_str[32]; format_kb(used_kb, used_str);
        font_draw_text(cx + 16, card_y + 26, used_str, rgb565(240, 110, 50), FONT_STYLE_REGULAR);

        /* Card 2: Free Memory */
        int c2_x = cx + 8 + card_w + 8;
        fb_fill_rounded_rect(c2_x, card_y, card_w, 52, 4, rgb565(36, 40, 54));
        font_draw_text(c2_x + 8, card_y + 6, "FREE MEMORY", rgb565(140, 145, 165), FONT_STYLE_REGULAR);
        char free_str[32]; format_kb(free_kb, free_str);
        font_draw_text(c2_x + 8, card_y + 26, free_str, rgb565(40, 180, 100), FONT_STYLE_REGULAR);

        /* Card 3: Total RAM */
        int c3_x = c2_x + card_w + 8;
        fb_fill_rounded_rect(c3_x, card_y, card_w, 52, 4, rgb565(36, 40, 54));
        font_draw_text(c3_x + 8, card_y + 6, "TOTAL POOL", rgb565(140, 145, 165), FONT_STYLE_REGULAR);
        char tot_str[32]; format_kb(tot_kb, tot_str);
        font_draw_text(c3_x + 8, card_y + 26, tot_str, COLOR_WHITE, FONT_STYLE_REGULAR);

        /* Segmented Visual RAM Map Bar */
        int map_y = card_y + 62;
        font_draw_text(cx + 10, map_y, "Physical Memory Allocation Map", rgb565(200, 205, 225), FONT_STYLE_REGULAR);
        
        int bar_x = cx + 10;
        int bar_y = map_y + 18;
        int bar_w = cw - 20;
        int bar_h = 16;
        fb_fill_rounded_rect(bar_x, bar_y, bar_w, bar_h, 3, rgb565(18, 20, 26));

        int used_px = (bar_w * pct) / 100;
        if (used_px < 6 && pct > 0) used_px = 6;
        if (used_px > bar_w) used_px = bar_w;
        fb_fill_rounded_rect(bar_x, bar_y, used_px, bar_h, 3, theme_get_primary_accent());

        /* Real-Time Waveform Activity Graph */
        int graph_y = bar_y + 28;
        font_draw_text(cx + 10, graph_y, "Real-Time Memory Waveform (Live 500ms Tick)", rgb565(200, 205, 225), FONT_STYLE_REGULAR);

        int gx = cx + 10;
        int gy = graph_y + 18;
        int gw = cw - 20;
        int gh = ch - (gy - cy) - 10;
        if (gh > 30) {
            fb_fill_rounded_rect(gx, gy, gw, gh, 4, rgb565(18, 20, 26));
            fb_drawline(gx, gy, gx + gw - 1, gy, rgb565(40, 44, 58));
            fb_drawline(gx, gy + gh / 2, gx + gw - 1, gy + gh / 2, rgb565(32, 36, 48));
            fb_drawline(gx, gy + gh - 1, gx + gw - 1, gy + gh - 1, rgb565(40, 44, 58));

            /* Plot waveform bars */
            int bar_step = gw / MEMVIEW_SAMPLES;
            if (bar_step < 2) bar_step = 2;
            for (int i = 0; i < MEMVIEW_SAMPLES; i++) {
                int px = gx + i * bar_step;
                int val = st->history[i];
                int vh = (val * (gh - 8)) / 100;
                if (vh < 2) vh = 2;
                if (vh > gh - 4) vh = gh - 4;

                uint16_t col = (val > 80) ? rgb565(230, 60, 60) :
                               (val > 50) ? rgb565(240, 180, 40) :
                               theme_get_primary_accent();
                fb_fillrect(px, gy + gh - vh - 2, bar_step - 1, vh, col);
            }
        }

    } else {
        /* ================= 2. HEX INSPECTOR ================= */
        font_draw_text(cx + 10, cy + 38, "Hex Dump [W/S = Scroll 8B, U/D = Page 128B]", rgb565(140, 145, 165), FONT_STYLE_REGULAR);

        int num_lines = (ch - 60) / 16;
        uint32_t addr = st->base_addr;

        char buf[64];
        for (int l = 0; l < num_lines; l++) {
            int py = cy + 58 + l * 16;

            addr_to_hex(addr, buf);
            buf[8] = ':'; buf[9] = ' '; buf[10] = '\0';
            font_draw_text(cx + 10, py, buf, rgb565(80, 200, 140), FONT_STYLE_MONO);

            for (int i = 0; i < 8; i++) {
                uint8_t *ptr = (uint8_t *)(addr + i);
                char hx[3];
                byte_to_hex(*ptr, hx);
                hx[2] = '\0';
                font_draw_text(cx + 100 + i * 24, py, hx, COLOR_WHITE, FONT_STYLE_MONO);

                /* ASCII view */
                char c = *ptr;
                if (c < 32 || c > 126) c = '.';
                char cbuf[2] = {c, '\0'};
                font_draw_text(cx + 310 + i * 8, py, cbuf, rgb565(200, 210, 230), FONT_STYLE_MONO);
            }

            addr += 8;
        }
    }
}

void memview_mouse_click(struct window *win, int mx, int my, int button) {
    (void)button;
    memview_state_t *st = (memview_state_t *)win->app_data;
    if (!st) return;

    if (my >= 5 && my < 27) {
        if (mx >= 8 && mx < 128) {
            st->tab_mode = 0;
        } else if (mx >= 134 && mx < 239) {
            st->tab_mode = 1;
        }
    }
}

void memview_key_event(struct window *win, char c) {
    memview_state_t *st = (memview_state_t *)win->app_data;
    if (!st) return;

    if (c == 'w' || c == 'W') {
        st->base_addr -= 8;
    } else if (c == 's' || c == 'S') {
        st->base_addr += 8;
    } else if (c == 'u' || c == 'U') {
        st->base_addr -= 128;
    } else if (c == 'd' || c == 'D') {
        st->base_addr += 128;
    } else if (c == '\t') {
        st->tab_mode = !st->tab_mode;
    }
}
