/* ============================================================================
 * STAX — app_settings.c
 * System Settings & Window Control Panel
 * ============================================================================ */

#include "app_settings.h"
#include "wm.h"
#include "framebuffer.h"
#include "string.h"
#include "heap.h"
#include "font8x16.h"
#include "rtc.h"

extern void wm_bring_to_front(struct window *win);

sys_settings_t g_settings = {
    .show_boot_log_on_startup = 1,
    .boot_win_x = 200,
    .boot_win_y = 44,
    .boot_win_w = 560,
    .boot_win_h = 380,
    .time_format_24h = 1,
    .active_tab = 0
};

extern int bg_color_idx;

void settings_init(void) {
    g_settings.show_boot_log_on_startup = 1;
    g_settings.boot_win_x = 200;
    g_settings.boot_win_y = 44;
    g_settings.boot_win_w = 560;
    g_settings.boot_win_h = 380;
    g_settings.time_format_24h = 1;
    g_settings.active_tab = 0;
}

static void draw_btn(int bx, int by, int bw, int bh, const char *label, int active) {
    uint16_t bg = active ? rgb565(40, 120, 220) : rgb565(215, 215, 225);
    uint16_t fg = active ? COLOR_WHITE : COLOR_BLACK;
    fb_fillrect(bx, by, bw, bh, bg);
    fb_drawline(bx, by, bx + bw - 1, by, active ? rgb565(80, 160, 255) : COLOR_WHITE);
    fb_drawline(bx, by, bx, by + bh - 1, active ? rgb565(80, 160, 255) : COLOR_WHITE);
    fb_drawline(bx + bw - 1, by, bx + bw - 1, by + bh - 1, active ? rgb565(20, 70, 140) : rgb565(140, 140, 150));
    fb_drawline(bx, by + bh - 1, bx + bw - 1, by + bh - 1, active ? rgb565(20, 70, 140) : rgb565(140, 140, 150));
    
    int tlen = (int)strlen(label);
    int tx = bx + (bw - tlen * 8) / 2;
    int ty = by + (bh - 16) / 2;
    draw_text(tx, ty, label, fg);
}

void settings_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    (void)win;
    fb_fillrect(cx, cy, cw, ch, rgb565(240, 240, 245));

    /* Top Tab Bar */
    int tab_w = cw / 4;
    const char *tabs[] = {"Startup", "Display", "Date & Time", "Network"};
    for (int t = 0; t < 4; t++) {
        int tx = cx + t * tab_w;
        int is_act = (g_settings.active_tab == t);
        uint16_t tbg = is_act ? rgb565(240, 240, 245) : rgb565(210, 210, 220);
        fb_fillrect(tx, cy, tab_w, 28, tbg);
        fb_drawline(tx, cy, tx + tab_w - 1, cy, COLOR_WHITE);
        fb_drawline(tx, cy, tx, cy + 27, COLOR_WHITE);
        fb_drawline(tx + tab_w - 1, cy, tx + tab_w - 1, cy + 27, rgb565(160, 160, 170));
        if (!is_act) {
            fb_drawline(tx, cy + 27, tx + tab_w - 1, cy + 27, rgb565(160, 160, 170));
        }
        int slen = (int)strlen(tabs[t]);
        draw_text(tx + (tab_w - slen * 8) / 2, cy + 6, tabs[t], is_act ? rgb565(20, 50, 160) : rgb565(60, 60, 70));
    }
    fb_drawline(cx, cy + 28, cx + cw - 1, cy + 28, rgb565(180, 180, 190));

    int px = cx + 16;
    int py = cy + 40;

    if (g_settings.active_tab == 0) {
        /* Startup & Boot Log Settings */
        draw_text(px, py, "Boot Log Window Settings:", rgb565(20, 40, 100));
        py += 24;

        draw_text(px, py + 4, "Launch on OS Boot:", COLOR_BLACK);
        draw_btn(px + 180, py, 90, 24, "Enabled", g_settings.show_boot_log_on_startup == 1);
        draw_btn(px + 280, py, 90, 24, "Hidden", g_settings.show_boot_log_on_startup == 0);
        py += 36;

        draw_text(px, py + 4, "Window Position:", COLOR_BLACK);
        draw_btn(px + 180, py, 95, 24, "Left (200,44)", g_settings.boot_win_x == 200);
        draw_btn(px + 285, py, 95, 24, "Center (220,60)", g_settings.boot_win_x == 220);
        py += 36;

        draw_text(px, py + 4, "Live Control:", COLOR_BLACK);
        draw_btn(px + 180, py, 200, 24, "Toggle Boot Log Window", 0);
        py += 38;

        fb_fillrect(px, py, cw - 32, 44, rgb565(230, 235, 245));
        fb_drawline(px, py, px + cw - 33, py, rgb565(180, 190, 215));
        draw_text(px + 8, py + 6, "* Windows automatically avoid overlapping the top nav bar.", rgb565(60, 70, 90));
        draw_text(px + 8, py + 22, "* All desktop icons are placed on the left side (x: 18..180).", rgb565(60, 70, 90));

    } else if (g_settings.active_tab == 1) {
        /* Display & Wallpaper */
        draw_text(px, py, "Desktop Wallpaper & Appearance:", rgb565(20, 40, 100));
        py += 24;

        draw_text(px, py + 4, "Theme Color:", COLOR_BLACK);
        const char *colors[] = {"Blue", "Teal", "Black", "Red", "Gray"};
        for (int c = 0; c < 5; c++) {
            draw_btn(px + 130 + c * 52, py, 48, 24, colors[c], bg_color_idx == c);
        }
        py += 36;

        draw_text(px, py + 4, "Resolution:", COLOR_BLACK);
        draw_btn(px + 130, py, 110, 24, "800 x 600", fb_width == 800);
        draw_btn(px + 250, py, 110, 24, "1024 x 768", fb_width == 1024);
        py += 40;

        fb_fillrect(px, py, cw - 32, 40, rgb565(230, 235, 245));
        draw_text(px + 8, py + 12, "Hardware: ARM926EJ-S with PL110 16-bit TrueColor Framebuffer", rgb565(40, 50, 70));

    } else if (g_settings.active_tab == 2) {
        /* Date & Time Settings */
        draw_text(px, py, "Real-Time Clock & Timezone:", rgb565(20, 40, 100));
        py += 24;

        char live_dt[64];
        rtc_format_ist_full(live_dt, sizeof(live_dt));
        draw_text(px, py, "Live Time (IST):", COLOR_BLACK);
        draw_text(px + 140, py, live_dt, rgb565(0, 100, 20));
        py += 26;

        draw_text(px, py, "Timezone:", COLOR_BLACK);
        draw_text(px + 140, py, "Indian Standard Time (UTC+05:30, Mumbai)", rgb565(40, 40, 50));
        py += 26;

        draw_text(px, py, "Hardware RTC:", COLOR_BLACK);
        draw_text(px + 140, py, "ARM PrimeCell PL031 at 0x101E8000 (Synced)", rgb565(40, 40, 50));
        py += 32;

        draw_text(px, py + 4, "Clock Display:", COLOR_BLACK);
        draw_btn(px + 140, py, 110, 24, "24-Hour (14:30)", g_settings.time_format_24h == 1);
        draw_btn(px + 260, py, 120, 24, "12-Hour (02:30 PM)", g_settings.time_format_24h == 0);

    } else if (g_settings.active_tab == 3) {
        /* Network Settings */
        draw_text(px, py, "Network Adapter Information:", rgb565(20, 40, 100));
        py += 24;

        draw_text(px, py, "NIC Model :", COLOR_BLACK);
        draw_text(px + 120, py, "SMC91C111 100Mbps Ethernet Controller", rgb565(30, 40, 60));
        py += 22;

        draw_text(px, py, "IP Address:", COLOR_BLACK);
        draw_text(px + 120, py, "10.0.2.15 (DHCP / Static)", rgb565(0, 100, 20));
        py += 22;

        draw_text(px, py, "Gateway   :", COLOR_BLACK);
        draw_text(px + 120, py, "10.0.2.2", rgb565(30, 40, 60));
        py += 22;

        draw_text(px, py, "DNS Server:", COLOR_BLACK);
        draw_text(px + 120, py, "10.0.2.3", rgb565(30, 40, 60));
        py += 22;

        draw_text(px, py, "Status    :", COLOR_BLACK);
        draw_text(px + 120, py, "Connected (lwIP Stack Active)", rgb565(0, 120, 40));
    }
}

void settings_mouse_click(struct window *win, int mx, int my, int button) {
    (void)win;
    if (button != 1) return;

    /* Tabs click */
    if (my < 28) {
        int tab_w = win->width / 4;
        if (tab_w > 0) {
            int t = mx / tab_w;
            if (t >= 0 && t < 4) g_settings.active_tab = t;
        }
        return;
    }

    int px = 16;
    int py = 40;

    if (g_settings.active_tab == 0) {
        py += 24;
        /* Enabled / Hidden */
        if (my >= py && my < py + 24) {
            if (mx >= px + 180 && mx < px + 270) g_settings.show_boot_log_on_startup = 1;
            else if (mx >= px + 280 && mx < px + 370) g_settings.show_boot_log_on_startup = 0;
        }
        py += 36;
        /* Position */
        if (my >= py && my < py + 24) {
            if (mx >= px + 180 && mx < px + 275) {
                g_settings.boot_win_x = 200;
                g_settings.boot_win_y = 44;
            } else if (mx >= px + 285 && mx < px + 380) {
                g_settings.boot_win_x = 220;
                g_settings.boot_win_y = 60;
            }
        }
        py += 36;
        /* Toggle live Boot Log */
        if (my >= py && my < py + 24 && mx >= px + 180 && mx < px + 380) {
            extern struct window *window_list;
            struct window *curr = window_list;
            int found = 0;
            while (curr) {
                if (strcmp(curr->title, "Boot Log") == 0) {
                    if (curr->state == WM_STATE_HIDDEN || curr->state == WM_STATE_MINIMIZED) {
                        curr->state = WM_STATE_ACTIVE;
                        wm_bring_to_front(curr);
                    } else {
                        curr->state = WM_STATE_HIDDEN;
                    }
                    found = 1;
                    break;
                }
                curr = curr->next;
            }
            if (!found) {
                extern void gfx_console_draw_window(struct window*, int, int, int, int);
                extern void gfx_console_key_event(struct window*, char);
                extern void gfx_console_mouse_click(struct window*, int, int, int);
                extern void gfx_console_mouse_drag(struct window*, int, int);
                window_t *gw = wm_add_window(g_settings.boot_win_x, g_settings.boot_win_y,
                                             g_settings.boot_win_w, g_settings.boot_win_h,
                                             "Boot Log", gfx_console_draw_window);
                if (gw) {
                    gw->key_event = gfx_console_key_event;
                    gw->mouse_click = gfx_console_mouse_click;
                    gw->mouse_drag = gfx_console_mouse_drag;
                }
            }
        }
    } else if (g_settings.active_tab == 1) {
        py += 24;
        /* Theme colors */
        if (my >= py && my < py + 24) {
            for (int c = 0; c < 5; c++) {
                if (mx >= px + 130 + c * 52 && mx < px + 130 + c * 52 + 48) {
                    bg_color_idx = c;
                    break;
                }
            }
        }
        py += 36;
        /* Resolution */
        if (my >= py && my < py + 24) {
            if (mx >= px + 130 && mx < px + 240) {
                extern void fb_set_resolution(uint32_t, uint32_t);
                fb_set_resolution(800, 600);
            } else if (mx >= px + 250 && mx < px + 360) {
                extern void fb_set_resolution(uint32_t, uint32_t);
                fb_set_resolution(1024, 768);
            }
        }
    } else if (g_settings.active_tab == 2) {
        py += 24 + 26 + 26 + 32;
        /* 24h vs 12h */
        if (my >= py && my < py + 24) {
            if (mx >= px + 140 && mx < px + 250) g_settings.time_format_24h = 1;
            else if (mx >= px + 260 && mx < px + 380) g_settings.time_format_24h = 0;
        }
    }
}
