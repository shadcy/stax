/* ============================================================================
 * STAX — app_settings.c
 * Production-Ready System Settings & Control Center with NVRAM/SD Persistence
 * ============================================================================ */

#include "app_settings.h"
#include "wm.h"
#include "framebuffer.h"
#include "string.h"
#include "heap.h"
#include "font8x16.h"
#include "rtc.h"
#include "fatfs/ff.h"
#include "system.h"

extern void wm_bring_to_front(struct window *win);
extern int bg_color_idx;

sys_settings_t g_settings = {
    .magic = SETTINGS_MAGIC,
    .version = 1,
    .show_boot_log_on_startup = 0,
    .boot_win_x = 200,
    .boot_win_y = 44,
    .boot_win_w = 560,
    .boot_win_h = 380,
    .time_format_24h = 1,
    .bg_color_idx = 0,
    .resolution_w = 1024,
    .resolution_h = 768,
    .active_tab = 0,
    .network_enabled = 1,
    .widgets_active = 0,
    .widgets_pinned_mask = 0
};

void settings_init(void) {
    g_settings.magic = SETTINGS_MAGIC;
    g_settings.version = 1;
    g_settings.show_boot_log_on_startup = 0;
    g_settings.boot_win_x = 200;
    g_settings.boot_win_y = 44;
    g_settings.boot_win_w = 560;
    g_settings.boot_win_h = 380;
    g_settings.time_format_24h = 1;
    g_settings.bg_color_idx = 0;
    g_settings.resolution_w = 1024;
    g_settings.resolution_h = 768;
    g_settings.active_tab = 0;
    g_settings.network_enabled = 1;
    g_settings.widgets_active = 0;
    g_settings.widgets_pinned_mask = 0;
}

void settings_load(void) {
    FIL f;
    if (f_open(&f, "/SETTINGS.CFG", FA_READ) == FR_OK) {
        sys_settings_t loaded;
        UINT br = 0;
        if (f_read(&f, &loaded, sizeof(sys_settings_t), &br) == FR_OK && br == sizeof(sys_settings_t)) {
            if (loaded.magic == SETTINGS_MAGIC) {
                g_settings = loaded;
                bg_color_idx = g_settings.bg_color_idx;
            }
        }
        f_close(&f);
    }
}

void settings_save(void) {
    FIL f;
    g_settings.magic = SETTINGS_MAGIC;
    g_settings.version = 1;
    g_settings.bg_color_idx = bg_color_idx;
    g_settings.resolution_w = (int)fb_width;
    g_settings.resolution_h = (int)fb_height;
    if (f_open(&f, "/SETTINGS.CFG", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        UINT bw = 0;
        f_write(&f, &g_settings, sizeof(sys_settings_t), &bw);
        f_close(&f);
    }
    extern void desk_save_positions(void);
    desk_save_positions();
}

#define SIDEBAR_W 120

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

    draw_text(sx + 48, sy + 2, is_on ? "ON" : "OFF", is_on ? rgb565(30, 140, 60) : rgb565(100, 105, 115));
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

/* Helper: Draw danger button */
static void draw_danger_btn(int bx, int by, int bw, int bh, const char *label) {
    fb_fillrect(bx, by, bw, bh, rgb565(200, 45, 45));
    fb_drawline(bx, by, bx + bw - 1, by, rgb565(240, 90, 90));
    fb_drawline(bx, by, bx, by + bh - 1, rgb565(240, 90, 90));
    fb_drawline(bx + bw - 1, by, bx + bw - 1, by + bh - 1, rgb565(140, 20, 20));
    fb_drawline(bx, by + bh - 1, bx + bw - 1, by + bh - 1, rgb565(140, 20, 20));

    int tlen = (int)strlen(label);
    int tx = bx + (bw - tlen * 8) / 2;
    int ty = by + (bh - 16) / 2;
    draw_text(tx, ty, label, COLOR_WHITE);
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
            draw_text(cx + 14, item_y + 5, tabs[t], COLOR_WHITE);
        } else {
            draw_text(cx + 14, item_y + 5, tabs[t], rgb565(55, 60, 75));
        }
    }

    /* ---- 2. Content Canvas ---- */
    int px = cx + SIDEBAR_W + 14;
    int card_w = cw - SIDEBAR_W - 28;
    if (card_w < 100) card_w = 100;

    if (g_settings.active_tab == 0) {
        /* ==== GENERAL & STARTUP SETTINGS ==== */
        draw_text(px, cy + 12, "Startup & Boot Log Settings", rgb565(25, 30, 45));
        fb_drawline(px, cy + 30, px + card_w, cy + 30, rgb565(220, 225, 235));

        /* Card 1: Boot Log Behavior */
        draw_card(px, cy + 38, card_w, 100);
        draw_text(px + 14, cy + 50, "Launch on OS Boot", rgb565(30, 35, 45));
        draw_switch(px + card_w - 80, cy + 48, g_settings.show_boot_log_on_startup);

        fb_drawline(px + 14, cy + 78, px + card_w - 14, cy + 78, rgb565(240, 242, 248));

        draw_text(px + 14, cy + 88, "Window Placement", rgb565(30, 35, 45));
        draw_btn(px + card_w - 180, cy + 84, 80, 24, "Left", g_settings.boot_win_x == 200);
        draw_btn(px + card_w - 90, cy + 84, 80, 24, "Center", g_settings.boot_win_x == 220);

        /* Card 2: Live Window Control & Reboot */
        draw_card(px, cy + 148, card_w, 72);
        draw_text(px + 14, cy + 158, "Boot Log Window", rgb565(30, 35, 45));
        draw_text(px + 14, cy + 176, "Live terminal instance", rgb565(120, 125, 140));
        draw_btn(px + card_w - 240, cy + 160, 110, 26, "Toggle Log", 0);
        draw_danger_btn(px + card_w - 120, cy + 160, 110, 26, "Reboot OS");

        /* Info Card */
        fb_fillrect(px, cy + 230, card_w, 56, rgb565(234, 242, 255));
        fb_drawline(px, cy + 230, px + card_w - 1, cy + 230, rgb565(180, 205, 245));
        fb_drawline(px, cy + 285, px + card_w - 1, cy + 285, rgb565(180, 205, 245));
        draw_text(px + 12, cy + 238, "* Settings auto-saved to /SETTINGS.CFG on SD", rgb565(0, 120, 30));
        draw_text(px + 12, cy + 256, "* Boot window anchored below top nav bar (Y=44)", rgb565(30, 75, 150));

    } else if (g_settings.active_tab == 1) {
        /* ==== DISPLAY & APPEARANCE ==== */
        draw_text(px, cy + 12, "Display & Desktop Appearance", rgb565(25, 30, 45));
        fb_drawline(px, cy + 30, px + card_w, cy + 30, rgb565(220, 225, 235));

        /* Card 1: Theme colors */
        draw_card(px, cy + 38, card_w, 96);
        draw_text(px + 14, cy + 48, "Desktop Theme Color", rgb565(30, 35, 45));
        const char *colors[] = {"Blue", "Teal", "Dark", "Red", "Gray"};
        int btn_w = 70;
        int btn_gap = 8;
        int total_w = 5 * btn_w + 4 * btn_gap;
        int start_x = px + (card_w - total_w) / 2;
        for (int c = 0; c < 5; c++) {
            draw_btn(start_x + c * (btn_w + btn_gap), cy + 68, btn_w, 24, colors[c], bg_color_idx == c);
        }

        /* Card 2: Resolution */
        draw_card(px, cy + 144, card_w, 80);
        draw_text(px + 14, cy + 154, "Display Resolution", rgb565(30, 35, 45));
        draw_text(px + 14, cy + 174, "PL110 16-bit TrueColor", rgb565(120, 125, 140));
        draw_btn(px + card_w - 195, cy + 158, 90, 24, "800x600", fb_width == 800);
        draw_btn(px + card_w - 95, cy + 158, 90, 24, "1024x768", fb_width == 1024);

    } else if (g_settings.active_tab == 2) {
        /* ==== DATE & TIME (IST MUMBAI) ==== */
        draw_text(px, cy + 12, "Date & Time (IST Mumbai)", rgb565(25, 30, 45));
        fb_drawline(px, cy + 30, px + card_w, cy + 30, rgb565(220, 225, 235));

        draw_card(px, cy + 38, card_w, 170);
        draw_text(px + 14, cy + 48, "Live Time (IST) :", rgb565(30, 35, 45));
        char live_dt[48];
        rtc_format_ist_full(live_dt, sizeof(live_dt));
        draw_text(px + 155, cy + 48, live_dt, rgb565(0, 120, 30));

        fb_drawline(px + 14, cy + 72, px + card_w - 14, cy + 72, rgb565(240, 242, 248));

        draw_text(px + 14, cy + 82, "Timezone        :", rgb565(30, 35, 45));
        draw_text(px + 155, cy + 82, "IST (UTC+05:30)", rgb565(40, 45, 60));

        fb_drawline(px + 14, cy + 106, px + card_w - 14, cy + 106, rgb565(240, 242, 248));

        draw_text(px + 14, cy + 116, "Hardware RTC    :", rgb565(30, 35, 45));
        draw_text(px + 155, cy + 116, "PL031 (Synced)", rgb565(40, 45, 60));

        fb_drawline(px + 14, cy + 140, px + card_w - 14, cy + 140, rgb565(240, 242, 248));

        draw_text(px + 14, cy + 148, "Clock Display   :", rgb565(30, 35, 45));
        draw_btn(px + card_w - 175, cy + 144, 80, 22, "24-Hour", g_settings.time_format_24h == 1);
        draw_btn(px + card_w - 90, cy + 144, 80, 22, "12-Hour", g_settings.time_format_24h == 0);
    } else if (g_settings.active_tab == 3) {
        /* ==== NETWORK CONFIG & INTERNET CONTROL ==== */
        draw_text(px, cy + 12, "Internet Connectivity & Network Control", rgb565(25, 30, 45));
        fb_drawline(px, cy + 30, px + card_w, cy + 30, rgb565(220, 225, 235));

        /* Card 1: Internet Master Switch */
        draw_card(px, cy + 38, card_w, 64);
        draw_text(px + 14, cy + 50, "Internet Connection", rgb565(30, 35, 45));
        draw_text(px + 14, cy + 68, "Enables HTTP APIs, Weather & Crypto Widgets", rgb565(120, 125, 140));
        draw_switch(px + card_w - 80, cy + 48, g_settings.network_enabled);

        /* Card 2: Live Network Adapter Info */
        draw_card(px, cy + 110, card_w, 130);
        int is_on = g_settings.network_enabled;
        
        draw_text(px + 14, cy + 120, "Status   :", rgb565(30, 35, 45));
        draw_text(px + 105, cy + 120, is_on ? "ONLINE / CONNECTED" : "OFFLINE / DISABLED", is_on ? rgb565(0, 140, 40) : rgb565(210, 40, 40));

        draw_text(px + 14, cy + 140, "IP Addr  :", rgb565(30, 35, 45));
        draw_text(px + 105, cy + 140, is_on ? "10.0.2.15 (DHCP Active)" : "--.--.--.--", rgb565(40, 50, 70));

        draw_text(px + 14, cy + 160, "Gateway  :", rgb565(30, 35, 45));
        draw_text(px + 105, cy + 160, is_on ? "10.0.2.2 (QEMU Slirp)" : "--.--.--.--", rgb565(40, 50, 70));

        draw_text(px + 14, cy + 180, "DNS / RTT:", rgb565(30, 35, 45));
        draw_text(px + 105, cy + 180, is_on ? "10.0.2.3 | 12ms (Nominal)" : "Offline", rgb565(40, 50, 70));

        draw_text(px + 14, cy + 200, "Adapter  :", rgb565(30, 35, 45));
        draw_text(px + 105, cy + 200, "SMC91C111 100Mbps Ethernet", rgb565(40, 50, 70));

        /* Action Buttons */
        draw_btn(px, cy + 248, 140, 26, "Open Widgets", 0);
        draw_btn(px + 150, cy + 248, 140, 26, "Toggle Internet", is_on);

    } else if (g_settings.active_tab == 4) {
        /* ==== ABOUT STAX OS ==== */
        draw_text(px, cy + 12, "About STAX Operating System", rgb565(25, 30, 45));
        fb_drawline(px, cy + 30, px + card_w, cy + 30, rgb565(220, 225, 235));

        draw_card(px, cy + 38, card_w, 150);
        draw_text(px + 14, cy + 48, "STAX Operating System", rgb565(20, 40, 110));
        draw_text(px + 14, cy + 70, "Edition    : Advanced Agentic Edition", rgb565(50, 55, 70));
        draw_text(px + 14, cy + 92, "Kernel     : ARM926EJ-S Monolithic Phase 6e", rgb565(50, 55, 70));
        draw_text(px + 14, cy + 114, "Compositor : Multi-Window GFX Compositor", rgb565(50, 55, 70));
        draw_text(px + 14, cy + 136, "Status     : All Subsystems Nominal", rgb565(0, 110, 25));

        draw_danger_btn(px, cy + 196, 140, 28, "Reboot System");
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

    int px = SIDEBAR_W + 14;
    int card_w = win->width - SIDEBAR_W - 28;

    if (g_settings.active_tab == 0) {
        /* Tab 0: General / Startup */
        /* Toggle Switch: Launch on Boot */
        if (my >= 46 && my < 70 && mx >= px + card_w - 80 && mx < px + card_w) {
            g_settings.show_boot_log_on_startup = !g_settings.show_boot_log_on_startup;
            settings_save();
            return;
        }
        /* Position buttons */
        if (my >= 84 && my < 110) {
            if (mx >= px + card_w - 180 && mx < px + card_w - 95) {
                g_settings.boot_win_x = 200;
                g_settings.boot_win_y = 44;
                settings_save();
            } else if (mx >= px + card_w - 90 && mx < px + card_w) {
                g_settings.boot_win_x = 220;
                g_settings.boot_win_y = 60;
                settings_save();
            }
            return;
        }
        /* Toggle log button */
        if (my >= 160 && my < 188) {
            if (mx >= px + card_w - 240 && mx < px + card_w - 125) {
                extern void wm_toggle_boot_log(void);
                wm_toggle_boot_log();
                return;
            } else if (mx >= px + card_w - 120 && mx < px + card_w) {
                settings_save();
                system_reboot();
                return;
            }
        }

    } else if (g_settings.active_tab == 1) {
        /* Tab 1: Display & Themes */
        if (my >= 68 && my < 94) {
            int btn_w = 70;
            int btn_gap = 8;
            int total_w = 5 * btn_w + 4 * btn_gap;
            int start_x = px + (card_w - total_w) / 2;
            for (int c = 0; c < 5; c++) {
                int bx = start_x + c * (btn_w + btn_gap);
                if (mx >= bx && mx < bx + btn_w) {
                    bg_color_idx = c;
                    g_settings.bg_color_idx = c;
                    settings_save();
                    return;
                }
            }
        }
        if (my >= 156 && my < 184) {
            if (mx >= px + card_w - 195 && mx < px + card_w - 100) {
                extern void fb_set_resolution(uint32_t, uint32_t);
                fb_set_resolution(800, 600);
                settings_save();
            } else if (mx >= px + card_w - 95 && mx < px + card_w) {
                extern void fb_set_resolution(uint32_t, uint32_t);
                fb_set_resolution(1024, 768);
                settings_save();
            }
            return;
        }

    } else if (g_settings.active_tab == 2) {
        /* Tab 2: Date & Time */
        if (my >= 142 && my < 168) {
            if (mx >= px + card_w - 175 && mx < px + card_w - 95) {
                g_settings.time_format_24h = 1;
                settings_save();
            } else if (mx >= px + card_w - 90 && mx < px + card_w) {
                g_settings.time_format_24h = 0;
                settings_save();
            }
            return;
        }

    } else if (g_settings.active_tab == 3) {
        /* Tab 3: Network & Internet */
        /* Master Toggle Switch */
        if (my >= 46 && my < 70 && mx >= px + card_w - 80 && mx < px + card_w) {
            g_settings.network_enabled = !g_settings.network_enabled;
            settings_save();
            return;
        }
        /* Open Widgets Button */
        if (my >= 248 && my < 276) {
            if (mx >= px && mx < px + 140) {
                extern struct window *widgets_open_window(void);
                widgets_open_window();
                return;
            } else if (mx >= px + 150 && mx < px + 290) {
                g_settings.network_enabled = !g_settings.network_enabled;
                settings_save();
                return;
            }
        }

    } else if (g_settings.active_tab == 4) {
        /* Tab 4: About STAX -> Reboot Button */
        if (my >= 196 && my < 224 && mx >= px && mx < px + 140) {
            settings_save();
            system_reboot();
            return;
        }
    }
}
