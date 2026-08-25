/* ============================================================================
 * STAX — app_settings.c
 * Production-Ready System Settings & Control Center
 * ============================================================================ */

#include "app_settings.h"
#include "wm.h"
#include "framebuffer.h"
#include "string.h"
#include "heap.h"
#include "font8x16.h"
#include "rtc.h"

extern void wm_bring_to_front(struct window *win);
extern int bg_color_idx;

sys_settings_t g_settings = {
    .show_boot_log_on_startup = 1,
    .boot_win_x = 200,
    .boot_win_y = 44,
    .boot_win_w = 560,
    .boot_win_h = 380,
    .time_format_24h = 1,
    .active_tab = 0
};

void settings_init(void) {
    g_settings.show_boot_log_on_startup = 1;
    g_settings.boot_win_x = 200;
    g_settings.boot_win_y = 44;
    g_settings.boot_win_w = 560;
    g_settings.boot_win_h = 380;
    g_settings.time_format_24h = 1;
    g_settings.active_tab = 0;
}

#define SIDEBAR_W 130

/* Helper: Draw toggle pill switch */
static void draw_switch(int sx, int sy, int is_on) {
    uint16_t track_col = is_on ? rgb565(50, 180, 80) : rgb565(170, 175, 185);
    fb_fillrect(sx, sy, 44, 20, track_col);
    fb_drawline(sx, sy, sx + 43, sy, is_on ? rgb565(70, 200, 100) : rgb565(190, 195, 205));
    fb_drawline(sx, sy + 19, sx + 43, sy + 19, is_on ? rgb565(30, 140, 60) : rgb565(140, 145, 155));

    int knob_x = is_on ? (sx + 24) : (sx + 2);
    fb_fillrect(knob_x, sy + 2, 18, 16, COLOR_WHITE);
    fb_drawline(knob_x, sy + 2, knob_x + 17, sy + 2, COLOR_WHITE);
    fb_drawline(knob_x + 17, sy + 2, knob_x + 17, sy + 17, rgb565(180, 180, 190));
    fb_drawline(knob_x, sy + 17, knob_x + 17, sy + 17, rgb565(180, 180, 190));

    draw_text(sx + 50, sy + 2, is_on ? "ON" : "OFF", is_on ? rgb565(30, 140, 60) : rgb565(100, 105, 115));
}

/* Helper: Draw action button */
static void draw_btn(int bx, int by, int bw, int bh, const char *label, int active) {
    uint16_t bg = active ? rgb565(35, 110, 225) : rgb565(225, 228, 235);
    uint16_t fg = active ? COLOR_WHITE : rgb565(30, 35, 45);
    fb_fillrect(bx, by, bw, bh, bg);
    fb_drawline(bx, by, bx + bw - 1, by, active ? rgb565(70, 150, 255) : COLOR_WHITE);
    fb_drawline(bx, by, bx, by + bh - 1, active ? rgb565(70, 150, 255) : COLOR_WHITE);
    fb_drawline(bx + bw - 1, by, bx + bw - 1, by + bh - 1, active ? rgb565(20, 70, 160) : rgb565(160, 165, 175));
    fb_drawline(bx, by + bh - 1, bx + bw - 1, by + bh - 1, active ? rgb565(20, 70, 160) : rgb565(160, 165, 175));

    int tlen = (int)strlen(label);
    int tx = bx + (bw - tlen * 8) / 2;
    int ty = by + (bh - 16) / 2;
    draw_text(tx, ty, label, fg);
}

/* Helper: Draw card group container */
static void draw_card(int x, int y, int w, int h) {
    fb_fillrect(x, y, w, h, COLOR_WHITE);
    fb_drawline(x, y, x + w - 1, y, rgb565(215, 220, 230));
    fb_drawline(x, y, x, y + h - 1, rgb565(215, 220, 230));
    fb_drawline(x + w - 1, y, x + w - 1, y + h - 1, rgb565(205, 210, 220));
    fb_drawline(x, y + h - 1, x + w - 1, y + h - 1, rgb565(205, 210, 220));
}

void settings_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    (void)win;

    /* Main background */
    fb_fillrect(cx, cy, cw, ch, rgb565(245, 246, 250));

    /* ---- 1. Sidebar Navigation ---- */
    fb_fillrect(cx, cy, SIDEBAR_W, ch, rgb565(232, 235, 242));
    fb_drawline(cx + SIDEBAR_W - 1, cy, cx + SIDEBAR_W - 1, cy + ch - 1, rgb565(205, 210, 220));

    /* Sidebar Title */
    draw_text(cx + 12, cy + 12, "Settings", rgb565(40, 45, 60));
    fb_drawline(cx + 10, cy + 32, cx + SIDEBAR_W - 10, cy + 32, rgb565(210, 215, 225));

    const char *tabs[] = {"General", "Display", "Date & Time", "Network", "About STAX"};
    for (int t = 0; t < 5; t++) {
        int item_y = cy + 42 + t * 34;
        int is_sel = (g_settings.active_tab == t);
        if (is_sel) {
            fb_fillrect(cx + 8, item_y, SIDEBAR_W - 16, 26, rgb565(40, 115, 225));
            fb_drawline(cx + 8, item_y, cx + SIDEBAR_W - 9, item_y, rgb565(75, 145, 255));
            draw_text(cx + 16, item_y + 5, tabs[t], COLOR_WHITE);
        } else {
            draw_text(cx + 16, item_y + 5, tabs[t], rgb565(55, 60, 75));
        }
    }

    /* ---- 2. Content Canvas ---- */
    int px = cx + SIDEBAR_W + 16;
    int card_w = cw - SIDEBAR_W - 32;
    if (card_w < 100) card_w = 100;

    if (g_settings.active_tab == 0) {
        /* ==== GENERAL & STARTUP SETTINGS ==== */
        draw_text(px, cy + 12, "Startup & Boot Log Settings", rgb565(25, 30, 45));
        fb_drawline(px, cy + 30, px + card_w, cy + 30, rgb565(220, 225, 235));

        /* Card 1: Boot Log Behavior */
        draw_card(px, cy + 38, card_w, 92);
        draw_text(px + 12, cy + 48, "Boot Log Window on Boot", rgb565(30, 35, 45));
        draw_switch(px + card_w - 90, cy + 46, g_settings.show_boot_log_on_startup);

        fb_drawline(px + 12, cy + 76, px + card_w - 12, cy + 76, rgb565(240, 242, 248));

        draw_text(px + 12, cy + 86, "Startup Placement", rgb565(30, 35, 45));
        draw_btn(px + card_w - 190, cy + 82, 85, 22, "Left (200)", g_settings.boot_win_x == 200);
        draw_btn(px + card_w - 95, cy + 82, 85, 22, "Center (220)", g_settings.boot_win_x == 220);

        /* Card 2: Live Window Control */
        draw_card(px, cy + 138, card_w, 64);
        draw_text(px + 12, cy + 148, "Terminal & Log Window", rgb565(30, 35, 45));
        draw_text(px + 12, cy + 166, "Bring Boot Log to front or toggle visibility", rgb565(120, 125, 140));
        draw_btn(px + card_w - 150, cy + 158, 140, 26, "Toggle Boot Log", 0);

        /* Info Card */
        fb_fillrect(px, cy + 210, card_w, 56, rgb565(232, 240, 254));
        fb_drawline(px, cy + 210, px + card_w - 1, cy + 210, rgb565(180, 205, 245));
        fb_drawline(px, cy + 265, px + card_w - 1, cy + 265, rgb565(180, 205, 245));
        draw_text(px + 10, cy + 218, "* Boot window is anchored at Y=44 (below top bar).", rgb565(30, 70, 140));
        draw_text(px + 10, cy + 236, "* Left columns (X: 18..180) are reserved for app icons.", rgb565(30, 70, 140));

    } else if (g_settings.active_tab == 1) {
        /* ==== DISPLAY & APPEARANCE ==== */
        draw_text(px, cy + 12, "Display & Desktop Appearance", rgb565(25, 30, 45));
        fb_drawline(px, cy + 30, px + card_w, cy + 30, rgb565(220, 225, 235));

        /* Card 1: Theme colors */
        draw_card(px, cy + 38, card_w, 92);
        draw_text(px + 12, cy + 48, "Desktop Wallpaper Color", rgb565(30, 35, 45));
        const char *colors[] = {"Blue", "Teal", "Dark", "Red", "Gray"};
        for (int c = 0; c < 5; c++) {
            int bx = px + 12 + c * ((card_w - 24) / 5);
            int bw = (card_w - 24) / 5 - 4;
            draw_btn(bx, cy + 68, bw, 24, colors[c], bg_color_idx == c);
        }

        /* Card 2: Resolution */
        draw_card(px, cy + 138, card_w, 80);
        draw_text(px + 12, cy + 148, "Display Resolution", rgb565(30, 35, 45));
        draw_text(px + 12, cy + 168, "Current: PL110 16-bit TrueColor FB", rgb565(120, 125, 140));
        draw_btn(px + card_w - 180, cy + 152, 80, 24, "800x600", fb_width == 800);
        draw_btn(px + card_w - 90, cy + 152, 80, 24, "1024x768", fb_width == 1024);

    } else if (g_settings.active_tab == 2) {
        /* ==== DATE & TIME (IST MUMBAI) ==== */
        draw_text(px, cy + 12, "Date & Time (IST Mumbai)", rgb565(25, 30, 45));
        fb_drawline(px, cy + 30, px + card_w, cy + 30, rgb565(220, 225, 235));

        draw_card(px, cy + 38, card_w, 148);
        draw_text(px + 12, cy + 48, "Live Synced Time", rgb565(30, 35, 45));
        char live_dt[48];
        rtc_format_ist_full(live_dt, sizeof(live_dt));
        draw_text(px + 150, cy + 48, live_dt, rgb565(0, 120, 30));

        fb_drawline(px + 12, cy + 70, px + card_w - 12, cy + 70, rgb565(240, 242, 248));

        draw_text(px + 12, cy + 78, "Timezone", rgb565(30, 35, 45));
        draw_text(px + 150, cy + 78, "IST (UTC+05:30, Mumbai)", rgb565(40, 45, 60));

        fb_drawline(px + 12, cy + 100, px + card_w - 12, cy + 100, rgb565(240, 242, 248));

        draw_text(px + 12, cy + 108, "Hardware RTC", rgb565(30, 35, 45));
        draw_text(px + 150, cy + 108, "PL031 at 0x101E8000 (Active)", rgb565(40, 45, 60));

        fb_drawline(px + 12, cy + 130, px + card_w - 12, cy + 130, rgb565(240, 242, 248));

        draw_text(px + 12, cy + 138, "Clock Mode", rgb565(30, 35, 45));
        draw_btn(px + card_w - 170, cy + 134, 75, 22, "24-Hour", g_settings.time_format_24h == 1);
        draw_btn(px + card_w - 90, cy + 134, 75, 22, "12-Hour", g_settings.time_format_24h == 0);

    } else if (g_settings.active_tab == 3) {
        /* ==== NETWORK CONFIG ==== */
        draw_text(px, cy + 12, "Network Adapter Information", rgb565(25, 30, 45));
        fb_drawline(px, cy + 30, px + card_w, cy + 30, rgb565(220, 225, 235));

        draw_card(px, cy + 38, card_w, 140);
        draw_text(px + 12, cy + 48, "Ethernet Adapter :", rgb565(30, 35, 45));
        draw_text(px + 170, cy + 48, "SMC91C111 100Mbps", rgb565(40, 50, 70));

        draw_text(px + 12, cy + 72, "IP Address       :", rgb565(30, 35, 45));
        draw_text(px + 170, cy + 72, "10.0.2.15 (DHCP / Static)", rgb565(0, 110, 25));

        draw_text(px + 12, cy + 96, "Gateway IP       :", rgb565(30, 35, 45));
        draw_text(px + 170, cy + 96, "10.0.2.2 (QEMU Slirp)", rgb565(40, 50, 70));

        draw_text(px + 12, cy + 120, "DNS Nameserver   :", rgb565(30, 35, 45));
        draw_text(px + 170, cy + 120, "10.0.2.3", rgb565(40, 50, 70));

    } else if (g_settings.active_tab == 4) {
        /* ==== ABOUT STAX OS ==== */
        draw_text(px, cy + 12, "About STAX Operating System", rgb565(25, 30, 45));
        fb_drawline(px, cy + 30, px + card_w, cy + 30, rgb565(220, 225, 235));

        draw_card(px, cy + 38, card_w, 150);
        draw_text(px + 14, cy + 50, "STAX OS - Advanced Agentic Edition", rgb565(20, 40, 110));
        draw_text(px + 14, cy + 72, "Kernel : ARM926EJ-S Monolithic Phase 6e", rgb565(50, 55, 70));
        draw_text(px + 14, cy + 94, "Stack  : lwIP TCP/IP, FatFs, Compositor", rgb565(50, 55, 70));
        draw_text(px + 14, cy + 116, "Display: PL110 16-bit Color Framebuffer", rgb565(50, 55, 70));
        draw_text(px + 14, cy + 138, "Clock  : Real-Time Synced (IST Mumbai)", rgb565(0, 110, 25));
    }
}

void settings_mouse_click(struct window *win, int mx, int my, int button) {
    if (button != 1) return;

    /* 1. Sidebar Tab Clicks */
    if (mx < SIDEBAR_W) {
        for (int t = 0; t < 5; t++) {
            int item_y = 42 + t * 34;
            if (my >= item_y && my < item_y + 28) {
                g_settings.active_tab = t;
                return;
            }
        }
        return;
    }

    int px = SIDEBAR_W + 16;
    int card_w = win->width - SIDEBAR_W - 32;

    if (g_settings.active_tab == 0) {
        /* Tab 0: General / Startup */
        /* Toggle Switch: Launch on Boot */
        if (my >= 46 && my < 68 && mx >= px + card_w - 90 && mx < px + card_w) {
            g_settings.show_boot_log_on_startup = !g_settings.show_boot_log_on_startup;
            return;
        }
        /* Position buttons */
        if (my >= 82 && my < 106) {
            if (mx >= px + card_w - 190 && mx < px + card_w - 100) {
                g_settings.boot_win_x = 200;
                g_settings.boot_win_y = 44;
            } else if (mx >= px + card_w - 95 && mx < px + card_w - 5) {
                g_settings.boot_win_x = 220;
                g_settings.boot_win_y = 60;
            }
            return;
        }
        /* Toggle Boot Log Window Button */
        if (my >= 158 && my < 186 && mx >= px + card_w - 150 && mx < px + card_w) {
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
            return;
        }

    } else if (g_settings.active_tab == 1) {
        /* Tab 1: Display & Themes */
        if (my >= 68 && my < 94) {
            for (int c = 0; c < 5; c++) {
                int bx = px + 12 + c * ((card_w - 24) / 5);
                int bw = (card_w - 24) / 5 - 4;
                if (mx >= bx && mx < bx + bw) {
                    bg_color_idx = c;
                    return;
                }
            }
        }
        if (my >= 152 && my < 178) {
            if (mx >= px + card_w - 180 && mx < px + card_w - 95) {
                extern void fb_set_resolution(uint32_t, uint32_t);
                fb_set_resolution(800, 600);
            } else if (mx >= px + card_w - 90 && mx < px + card_w) {
                extern void fb_set_resolution(uint32_t, uint32_t);
                fb_set_resolution(1024, 768);
            }
            return;
        }

    } else if (g_settings.active_tab == 2) {
        /* Tab 2: Date & Time */
        if (my >= 134 && my < 158) {
            if (mx >= px + card_w - 170 && mx < px + card_w - 95) {
                g_settings.time_format_24h = 1;
            } else if (mx >= px + card_w - 90 && mx < px + card_w) {
                g_settings.time_format_24h = 0;
            }
            return;
        }
    }
}
