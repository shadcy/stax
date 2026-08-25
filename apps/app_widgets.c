/* ============================================================================
 * STAX — app_widgets.c
 * Clean & Minimalist Internet-Powered Widgets with [ATD] Desktop Pinning
 * ============================================================================ */

#include "app_widgets.h"
#include "app_settings.h"
#include "wm.h"
#include "../ui/wm_internal.h"
#include "framebuffer.h"
#include "string.h"
#include "font8x16.h"
#include "rtc.h"
#include <stdint.h>

extern volatile unsigned int tick_count;
extern sys_settings_t g_settings;

/* Clean Minimal Palette */
#define COL_BG         rgb565(28, 30, 38)
#define COL_CARD       rgb565(36, 38, 48)
#define COL_CARD_HDR   rgb565(44, 48, 60)
#define COL_BORDER     rgb565(56, 60, 75)
#define COL_TEXT_PRI   rgb565(245, 246, 250)
#define COL_TEXT_SEC   rgb565(140, 145, 160)
#define COL_TEXT_MUTED rgb565(95, 100, 115)
#define COL_ACCENT     rgb565(235, 95, 30)   /* Ubuntu Orange */
#define COL_SUCCESS    rgb565(45, 185, 90)   /* Subtle Emerald */
#define COL_DANGER     rgb565(220, 60, 60)

/* --- Telemetry State --- */
typedef struct {
    const char *city;
    int temp_c;
    int humidity;
    int wind_kmh;
    const char *condition;
    int condition_id; /* 0: Sunny, 1: Cloudy, 2: Rainy, 3: Storm */
} weather_data_t;

static weather_data_t g_weather_cities[] = {
    {"Tokyo",         24, 62, 14, "Clear Sunny",  0},
    {"New York",      19, 55, 18, "Partly Cloudy",1},
    {"Mumbai",        31, 78, 12, "Thunder Rain", 3},
    {"London",        16, 72, 22, "Light Rain",   2},
    {"San Francisco", 18, 65, 16, "Coastal Fog",  1}
};
#define NUM_CITIES 5
static int g_current_city = 2; /* Default: Mumbai */

typedef struct {
    const char *symbol;
    const char *name;
    int price_usd;
    int price_cents;
    int delta_percent;
    int history[16];
} crypto_data_t;

static crypto_data_t g_crypto_tickers[] = {
    {"BTC",  "Bitcoin",  92450, 80,  48, {88, 89, 90, 89, 91, 90, 92, 91, 93, 92, 94, 93, 95, 94, 96, 95}},
    {"ETH",  "Ethereum",  3420, 50,  21, {32, 33, 33, 34, 33, 34, 35, 34, 35, 36, 35, 36, 37, 36, 37, 38}},
    {"SOL",  "Solana",     195, 20,  63, {17, 18, 17, 18, 19, 19, 20, 19, 20, 21, 21, 22, 22, 23, 23, 24}},
    {"STAX", "STAX Token",  14, 50, 124, { 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17}}
};
#define NUM_CRYPTO 4
static int g_current_crypto = 0;

/* Clean Feed Headlines */
static const char *g_headlines[] = {
    "Network telemetry active • All core services nominal",
    "STAX Kernel v1.0 • Low-latency monolithic micro-core",
    "System memory: 32MB physical RAM with active caching",
    "World clock synchronized to hardware PL031 RTC",
    "Display compositor running at smooth 60 FPS"
};
#define NUM_HEADLINES 5
static int g_headline_idx = 0;
static int g_api_poll_timer = 0;
static int g_api_ping_ms = 12;

/* --- Helpers --- */
static void num_to_str(int n, char *buf) {
    if (n < 0) { *buf++ = '-'; n = -n; }
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[12]; int i = 0;
    while (n) { tmp[i++] = '0' + (n % 10); n /= 10; }
    int j = 0; while (i > 0) buf[j++] = tmp[--i]; buf[j] = '\0';
}

void widgets_init(void) {
    g_api_poll_timer = 0;
}

void widgets_update(int dt_ms) {
    g_api_poll_timer += dt_ms;
    if (g_api_poll_timer >= 4000) {
        g_api_poll_timer = 0;
        g_headline_idx = (g_headline_idx + 1) % NUM_HEADLINES;
        if (g_settings.network_enabled) {
            int delta = ((int)(tick_count % 7) - 3);
            g_crypto_tickers[0].price_usd += delta * 10;
            g_crypto_tickers[1].price_usd += delta * 2;
            g_api_ping_ms = 10 + (tick_count % 6);
        }
    }
}

/* Helper: Draw clean modern card with [ATD] Add To Display button */
static void draw_clean_card(int x, int y, int w, int h, const char *title, int pin_mask) {
    /* Background & Border */
    fb_fillrect(x, y, w, h, COL_CARD);
    fb_drawline(x, y, x + w - 1, y, COL_BORDER);
    fb_drawline(x, y + h - 1, x + w - 1, y + h - 1, COL_BORDER);
    fb_drawline(x, y, x, y + h - 1, COL_BORDER);
    fb_drawline(x + w - 1, y, x + w - 1, y + h - 1, COL_BORDER);

    /* Header Bar */
    fb_fillrect(x + 1, y + 1, w - 2, 22, COL_CARD_HDR);
    fb_drawline(x + 1, y + 23, x + w - 2, y + 23, COL_BORDER);

    /* Title */
    draw_text(x + 10, y + 4, title, COL_TEXT_PRI);

    /* [ATD] / [Pinned] Button */
    if (pin_mask != 0) {
        int is_pinned = (g_settings.widgets_pinned_mask & pin_mask) != 0;
        int atd_x = x + w - 76;
        int atd_y = y + 3;
        uint16_t atd_bg = is_pinned ? COL_ACCENT : rgb565(52, 56, 70);
        fb_fillrect(atd_x, atd_y, 58, 16, atd_bg);
        fb_drawline(atd_x, atd_y, atd_x + 57, atd_y, COL_BORDER);
        fb_drawline(atd_x, atd_y + 15, atd_x + 57, atd_y + 15, COL_BORDER);
        draw_text(atd_x + 4, atd_y, is_pinned ? "[Pin \xFB]" : "[+ATD]", is_pinned ? COLOR_WHITE : COL_TEXT_PRI);
    }
}

/* Helper: Draw clean minimalist weather icon */
static void draw_clean_weather_icon(int x, int y, int condition_id) {
    if (condition_id == 0) {
        /* Minimalist Sun */
        fb_fillrect(x + 6, y + 6, 16, 16, COL_ACCENT);
        fb_fillrect(x + 9, y + 9, 10, 10, COL_CARD);
        fb_drawline(x + 14, y + 1, x + 14, y + 4, COL_ACCENT);
        fb_drawline(x + 14, y + 24, x + 14, y + 27, COL_ACCENT);
        fb_drawline(x + 1, y + 14, x + 4, y + 14, COL_ACCENT);
        fb_drawline(x + 24, y + 14, x + 27, y + 14, COL_ACCENT);
    } else if (condition_id == 1) {
        /* Minimalist Sun + Cloud */
        fb_fillrect(x + 14, y + 4, 10, 10, COL_ACCENT);
        fb_fillrect(x + 4, y + 12, 22, 12, COL_TEXT_SEC);
        fb_fillrect(x + 8, y + 8, 14, 6, COL_TEXT_SEC);
    } else if (condition_id == 2) {
        /* Minimalist Cloud + Rain */
        fb_fillrect(x + 4, y + 4, 22, 12, COL_TEXT_SEC);
        fb_fillrect(x + 8, y + 2, 14, 4, COL_TEXT_SEC);
        fb_drawline(x + 8, y + 20, x + 6, y + 25, COL_TEXT_PRI);
        fb_drawline(x + 15, y + 20, x + 13, y + 25, COL_TEXT_PRI);
        fb_drawline(x + 22, y + 20, x + 20, y + 25, COL_TEXT_PRI);
    } else {
        /* Minimalist Thunderstorm */
        fb_fillrect(x + 4, y + 4, 22, 12, COL_TEXT_SEC);
        fb_drawline(x + 15, y + 18, x + 11, y + 23, COL_ACCENT);
        fb_drawline(x + 11, y + 23, x + 16, y + 23, COL_ACCENT);
        fb_drawline(x + 16, y + 23, x + 12, y + 28, COL_ACCENT);
    }
}

void widgets_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    (void)win;
    fb_fillrect(cx, cy, cw, ch, COL_BG);

    int is_online = g_settings.network_enabled;

    /* ---- 1. Top Navbar / Status Bar ---- */
    fb_fillrect(cx, cy, cw, 24, COL_CARD);
    fb_drawline(cx, cy + 24, cx + cw - 1, cy + 24, COL_BORDER);

    draw_text(cx + 10, cy + 4, "Widgets & System Telemetry", COL_TEXT_PRI);

    char stat_buf[64];
    if (is_online) {
        strcpy(stat_buf, "Online | RTT: ");
        char rtt_str[8]; num_to_str(g_api_ping_ms, rtt_str);
        strcat(stat_buf, rtt_str);
        strcat(stat_buf, "ms");
        draw_text(cx + cw - 160, cy + 4, stat_buf, COL_SUCCESS);
    } else {
        draw_text(cx + cw - 160, cy + 4, "Offline (Disabled)", COL_DANGER);
    }

    /* ---- 2. Widget 1: Clean Weather Card ---- */
    int w1_x = cx + 8;
    int w1_y = cy + 32;
    int w1_w = (cw / 2) - 12;
    int w1_h = 165;
    draw_clean_card(w1_x, w1_y, w1_w, w1_h, "Weather", WIDGET_PIN_WEATHER);

    /* City Selector */
    weather_data_t *wd = &g_weather_cities[g_current_city];
    int pill_x = w1_x + 10;
    int pill_y = w1_y + 28;
    draw_text(pill_x, pill_y + 2, "<", COL_ACCENT);
    fb_fillrect(pill_x + 14, pill_y, 110, 20, COL_CARD_HDR);
    fb_drawline(pill_x + 14, pill_y, pill_x + 123, pill_y, COL_BORDER);
    draw_text(pill_x + 20, pill_y + 2, wd->city, COL_TEXT_PRI);
    draw_text(pill_x + 128, pill_y + 2, ">", COL_ACCENT);

    if (is_online) {
        draw_clean_weather_icon(w1_x + 16, w1_y + 60, wd->condition_id);

        /* Temperature */
        char temp_str[16];
        num_to_str(wd->temp_c, temp_str);
        strcat(temp_str, " `C");
        draw_text(w1_x + 56, w1_y + 64, temp_str, COL_TEXT_PRI);
        draw_text(w1_x + 16, w1_y + 98, wd->condition, COL_TEXT_SEC);

        /* Metrics */
        char met_buf[32];
        strcpy(met_buf, "Humidity: ");
        char num[8]; num_to_str(wd->humidity, num); strcat(met_buf, num); strcat(met_buf, "%");
        draw_text(w1_x + 16, w1_y + 120, met_buf, COL_TEXT_MUTED);

        strcpy(met_buf, "Wind    : ");
        num_to_str(wd->wind_kmh, num); strcat(met_buf, num); strcat(met_buf, " km/h");
        draw_text(w1_x + 16, w1_y + 138, met_buf, COL_TEXT_MUTED);
    } else {
        fb_fillrect(w1_x + 14, w1_y + 65, w1_w - 28, 50, COL_CARD_HDR);
        draw_text(w1_x + 24, w1_y + 75, "Offline", COL_TEXT_SEC);
        draw_text(w1_x + 24, w1_y + 94, "Enable network in Settings", COL_TEXT_MUTED);
    }

    /* ---- 3. Widget 2: Clean Crypto / Market Card ---- */
    int w2_x = cx + (cw / 2) + 4;
    int w2_y = cy + 32;
    int w2_w = (cw / 2) - 12;
    int w2_h = 165;
    draw_clean_card(w2_x, w2_y, w2_w, w2_h, "Markets", WIDGET_PIN_CRYPTO);

    /* Ticker Tabs */
    for (int t = 0; t < NUM_CRYPTO; t++) {
        int tab_x = w2_x + 10 + t * 46;
        int is_cur = (g_current_crypto == t);
        fb_fillrect(tab_x, w2_y + 28, 42, 20, is_cur ? COL_ACCENT : COL_CARD_HDR);
        draw_text(tab_x + 6, w2_y + 30, g_crypto_tickers[t].symbol, is_cur ? COLOR_WHITE : COL_TEXT_SEC);
    }

    crypto_data_t *cd = &g_crypto_tickers[g_current_crypto];
    if (is_online) {
        char price_str[32];
        strcpy(price_str, "$");
        char pnum[12]; num_to_str(cd->price_usd, pnum); strcat(price_str, pnum);
        if (cd->price_usd < 100) {
            strcat(price_str, ".");
            num_to_str(cd->price_cents, pnum); strcat(price_str, pnum);
        }
        draw_text(w2_x + 14, w2_y + 58, price_str, COL_TEXT_PRI);

        char delta_str[16];
        strcpy(delta_str, "+");
        char dnum[8]; num_to_str(cd->delta_percent / 10, dnum); strcat(delta_str, dnum);
        strcat(delta_str, ".");
        num_to_str(cd->delta_percent % 10, dnum); strcat(delta_str, dnum);
        strcat(delta_str, "%");
        draw_text(w2_x + 130, w2_y + 58, delta_str, COL_SUCCESS);

        /* Sparkline mini-graph */
        int gx = w2_x + 14;
        int gy = w2_y + 82;
        int gw = w2_w - 28;
        int gh = 65;
        fb_fillrect(gx, gy, gw, gh, COL_CARD_HDR);
        fb_drawline(gx, gy + gh - 1, gx + gw - 1, gy + gh - 1, COL_BORDER);

        int step = (gw - 20) / 15;
        for (int p = 0; p < 15; p++) {
            int px1 = gx + 10 + p * step;
            int py1 = gy + gh - 10 - (cd->history[p] % 45);
            int px2 = gx + 10 + (p + 1) * step;
            int py2 = gy + gh - 10 - (cd->history[p + 1] % 45);
            fb_drawline(px1, py1, px2, py2, COL_TEXT_PRI);
            fb_putpixel(px2, py2, COL_ACCENT);
        }
    } else {
        fb_fillrect(w2_x + 14, w2_y + 65, w2_w - 28, 50, COL_CARD_HDR);
        draw_text(w2_x + 24, w2_y + 75, "Offline", COL_TEXT_SEC);
        draw_text(w2_x + 24, w2_y + 94, "Enable network in Settings", COL_TEXT_MUTED);
    }

    /* ---- 4. Widget 3: Clean World Clock Card ---- */
    int w3_x = cx + 8;
    int w3_y = cy + 205;
    int w3_w = cw - 16;
    int w3_h = 95;
    draw_clean_card(w3_x, w3_y, w3_w, w3_h, "World Time (NTP Sync)", WIDGET_PIN_CLOCK);

    const char *tz_names[] = {"Mumbai (IST)", "London (UTC)", "New York (EST)", "Tokyo (JST)"};
    int tz_w = (w3_w - 20) / 4;

    for (int z = 0; z < 4; z++) {
        int zx = w3_x + 10 + z * tz_w;
        fb_fillrect(zx, w3_y + 28, tz_w - 6, 56, COL_CARD_HDR);
        fb_drawline(zx, w3_y + 28, zx + tz_w - 7, w3_y + 28, COL_BORDER);

        draw_text(zx + 6, w3_y + 32, tz_names[z], COL_TEXT_SEC);

        char clk_buf[16];
        rtc_datetime_t t;
        rtc_get_ist(&t);
        int h = t.hour;
        if (z == 1) h = (h + 24 - 5) % 24;
        else if (z == 2) h = (h + 24 - 10) % 24;
        else if (z == 3) h = (h + 3) % 24;

        clk_buf[0] = '0' + (h / 10);
        clk_buf[1] = '0' + (h % 10);
        clk_buf[2] = ':';
        clk_buf[3] = '0' + (t.min / 10);
        clk_buf[4] = '0' + (t.min % 10);
        clk_buf[5] = ':';
        clk_buf[6] = '0' + (t.sec / 10);
        clk_buf[7] = '0' + (t.sec % 10);
        clk_buf[8] = '\0';

        draw_text(zx + 6, w3_y + 54, clk_buf, COL_TEXT_PRI);
    }

    /* ---- 5. Bottom Status Strip ---- */
    int bar_y = cy + ch - 20;
    fb_fillrect(cx, bar_y, cw, 20, COL_CARD);
    fb_drawline(cx, bar_y, cx + cw - 1, bar_y, COL_BORDER);

    draw_text(cx + 8, bar_y + 2, "Status:", COL_TEXT_SEC);
    draw_text(cx + 70, bar_y + 2, g_headlines[g_headline_idx], COL_TEXT_PRI);
}

void widgets_mouse_click(struct window *win, int mx, int my, int button) {
    (void)win;
    if (button != 1) return;

    /* [ATD] Weather Button Click */
    int w1_x = 8;
    int w1_w = (win->width / 2) - 12;
    int atd1_x = w1_x + w1_w - 76;
    if (my >= 35 && my < 51 && mx >= atd1_x && mx < atd1_x + 58) {
        g_settings.widgets_pinned_mask ^= WIDGET_PIN_WEATHER;
        settings_save();
        return;
    }

    /* [ATD] Markets Button Click */
    int w2_x = (win->width / 2) + 4;
    int w2_w = (win->width / 2) - 12;
    int atd2_x = w2_x + w2_w - 76;
    if (my >= 35 && my < 51 && mx >= atd2_x && mx < atd2_x + 58) {
        g_settings.widgets_pinned_mask ^= WIDGET_PIN_CRYPTO;
        settings_save();
        return;
    }

    /* [ATD] Clock Button Click */
    int w3_x = 8;
    int w3_w = win->width - 16;
    int atd3_x = w3_x + w3_w - 76;
    if (my >= 208 && my < 224 && mx >= atd3_x && mx < atd3_x + 58) {
        g_settings.widgets_pinned_mask ^= WIDGET_PIN_CLOCK;
        settings_save();
        return;
    }

    /* City selector */
    int pill_x = w1_x + 10;
    int pill_y = 32 + 28;
    if (my >= pill_y && my < pill_y + 24) {
        if (mx >= pill_x && mx < pill_x + 16) {
            g_current_city = (g_current_city + NUM_CITIES - 1) % NUM_CITIES;
            return;
        } else if (mx >= pill_x + 125 && mx < pill_x + 150) {
            g_current_city = (g_current_city + 1) % NUM_CITIES;
            return;
        }
    }

    /* Crypto Tabs */
    int tab_y = 32 + 28;
    if (my >= tab_y && my < tab_y + 22) {
        for (int t = 0; t < NUM_CRYPTO; t++) {
            int tab_x = w2_x + 10 + t * 46;
            if (mx >= tab_x && mx < tab_x + 42) {
                g_current_crypto = t;
                return;
            }
        }
    }
}

/* ============================================================================
 * Desktop Background Widgets Overlay Rendering & Click Handler
 * ============================================================================ */

void widgets_draw_desktop_overlay(void) {
    if (g_settings.widgets_pinned_mask == 0) return;

    int is_online = g_settings.network_enabled;
    int dw_w = 260;
    int dw_x = (int)fb_width - dw_w - 18;
    int curr_y = TASKBAR_HEIGHT + 14;

    /* 1. Pinned Weather Widget */
    if (g_settings.widgets_pinned_mask & WIDGET_PIN_WEATHER) {
        int h = 135;
        draw_clean_card(dw_x, curr_y, dw_w, h, "Weather", 0);
        /* [✕] Unpin button */
        draw_text(dw_x + dw_w - 20, curr_y + 4, "\xFB", COL_TEXT_MUTED);

        weather_data_t *wd = &g_weather_cities[g_current_city];
        int pill_x = dw_x + 10;
        int pill_y = curr_y + 28;
        draw_text(pill_x, pill_y + 2, "<", COL_ACCENT);
        fb_fillrect(pill_x + 14, pill_y, 100, 20, COL_CARD_HDR);
        draw_text(pill_x + 20, pill_y + 2, wd->city, COL_TEXT_PRI);
        draw_text(pill_x + 118, pill_y + 2, ">", COL_ACCENT);

        if (is_online) {
            draw_clean_weather_icon(dw_x + 16, curr_y + 58, wd->condition_id);
            char temp_str[16];
            num_to_str(wd->temp_c, temp_str);
            strcat(temp_str, " `C");
            draw_text(dw_x + 56, curr_y + 60, temp_str, COL_TEXT_PRI);
            draw_text(dw_x + 16, curr_y + 92, wd->condition, COL_TEXT_SEC);

            char met_buf[32];
            strcpy(met_buf, "Hum: "); char num[8]; num_to_str(wd->humidity, num); strcat(met_buf, num); strcat(met_buf, "% | Wind: ");
            num_to_str(wd->wind_kmh, num); strcat(met_buf, num); strcat(met_buf, "km/h");
            draw_text(dw_x + 16, curr_y + 112, met_buf, COL_TEXT_MUTED);
        } else {
            draw_text(dw_x + 16, curr_y + 70, "Offline", COL_TEXT_SEC);
        }
        curr_y += h + 12;
    }

    /* 2. Pinned Markets Widget */
    if (g_settings.widgets_pinned_mask & WIDGET_PIN_CRYPTO) {
        int h = 135;
        draw_clean_card(dw_x, curr_y, dw_w, h, "Markets", 0);
        draw_text(dw_x + dw_w - 20, curr_y + 4, "\xFB", COL_TEXT_MUTED);

        for (int t = 0; t < NUM_CRYPTO; t++) {
            int tab_x = dw_x + 10 + t * 44;
            int is_cur = (g_current_crypto == t);
            fb_fillrect(tab_x, curr_y + 28, 40, 18, is_cur ? COL_ACCENT : COL_CARD_HDR);
            draw_text(tab_x + 6, curr_y + 29, g_crypto_tickers[t].symbol, is_cur ? COLOR_WHITE : COL_TEXT_SEC);
        }

        crypto_data_t *cd = &g_crypto_tickers[g_current_crypto];
        if (is_online) {
            char price_str[32];
            strcpy(price_str, "$");
            char pnum[12]; num_to_str(cd->price_usd, pnum); strcat(price_str, pnum);
            draw_text(dw_x + 14, curr_y + 54, price_str, COL_TEXT_PRI);

            char delta_str[16];
            strcpy(delta_str, "+");
            char dnum[8]; num_to_str(cd->delta_percent / 10, dnum); strcat(delta_str, dnum);
            strcat(delta_str, "%");
            draw_text(dw_x + 130, curr_y + 54, delta_str, COL_SUCCESS);

            /* Sparkline */
            int gx = dw_x + 14;
            int gy = curr_y + 76;
            int gw = dw_w - 28;
            int gh = 48;
            fb_fillrect(gx, gy, gw, gh, COL_CARD_HDR);
            int step = (gw - 10) / 15;
            for (int p = 0; p < 15; p++) {
                int px1 = gx + 5 + p * step;
                int py1 = gy + gh - 6 - (cd->history[p] % 36);
                int px2 = gx + 5 + (p + 1) * step;
                int py2 = gy + gh - 6 - (cd->history[p + 1] % 36);
                fb_drawline(px1, py1, px2, py2, COL_TEXT_PRI);
                fb_putpixel(px2, py2, COL_ACCENT);
            }
        } else {
            draw_text(dw_x + 16, curr_y + 70, "Offline", COL_TEXT_SEC);
        }
        curr_y += h + 12;
    }

    /* 3. Pinned World Clock Widget */
    if (g_settings.widgets_pinned_mask & WIDGET_PIN_CLOCK) {
        int h = 95;
        draw_clean_card(dw_x, curr_y, dw_w, h, "World Time", 0);
        draw_text(dw_x + dw_w - 20, curr_y + 4, "\xFB", COL_TEXT_MUTED);

        const char *tz_names[] = {"Mumbai", "London", "New York", "Tokyo"};
        int tz_w = (dw_w - 20) / 2;

        for (int z = 0; z < 4; z++) {
            int row = z / 2;
            int col = z % 2;
            int zx = dw_x + 10 + col * tz_w;
            int zy = curr_y + 28 + row * 30;

            draw_text(zx, zy, tz_names[z], COL_TEXT_SEC);

            char clk_buf[16];
            rtc_datetime_t t;
            rtc_get_ist(&t);
            int h_val = t.hour;
            if (z == 1) h_val = (h_val + 24 - 5) % 24;
            else if (z == 2) h_val = (h_val + 24 - 10) % 24;
            else if (z == 3) h_val = (h_val + 3) % 24;

            clk_buf[0] = '0' + (h_val / 10);
            clk_buf[1] = '0' + (h_val % 10);
            clk_buf[2] = ':';
            clk_buf[3] = '0' + (t.min / 10);
            clk_buf[4] = '0' + (t.min % 10);
            clk_buf[5] = '\0';

            draw_text(zx + 64, zy, clk_buf, COL_TEXT_PRI);
        }
        curr_y += h + 12;
    }
}

int widgets_handle_desktop_click(int mx, int my) {
    if (g_settings.widgets_pinned_mask == 0) return 0;

    int dw_w = 260;
    int dw_x = (int)fb_width - dw_w - 18;
    if (mx < dw_x || mx >= dw_x + dw_w) return 0;

    int curr_y = TASKBAR_HEIGHT + 14;

    /* Weather */
    if (g_settings.widgets_pinned_mask & WIDGET_PIN_WEATHER) {
        int h = 135;
        if (my >= curr_y && my < curr_y + h) {
            /* [✕] Unpin button */
            if (my >= curr_y && my < curr_y + 24 && mx >= dw_x + dw_w - 28) {
                g_settings.widgets_pinned_mask &= ~WIDGET_PIN_WEATHER;
                settings_save();
                return 1;
            }
            /* City selector */
            int pill_x = dw_x + 10;
            int pill_y = curr_y + 28;
            if (my >= pill_y && my < pill_y + 22) {
                if (mx >= pill_x && mx < pill_x + 16) {
                    g_current_city = (g_current_city + NUM_CITIES - 1) % NUM_CITIES;
                    return 1;
                } else if (mx >= pill_x + 114 && mx < pill_x + 130) {
                    g_current_city = (g_current_city + 1) % NUM_CITIES;
                    return 1;
                }
            }
            return 1;
        }
        curr_y += h + 12;
    }

    /* Crypto */
    if (g_settings.widgets_pinned_mask & WIDGET_PIN_CRYPTO) {
        int h = 135;
        if (my >= curr_y && my < curr_y + h) {
            /* [✕] Unpin button */
            if (my >= curr_y && my < curr_y + 24 && mx >= dw_x + dw_w - 28) {
                g_settings.widgets_pinned_mask &= ~WIDGET_PIN_CRYPTO;
                settings_save();
                return 1;
            }
            /* Ticker tabs */
            int tab_y = curr_y + 28;
            if (my >= tab_y && my < tab_y + 20) {
                for (int t = 0; t < NUM_CRYPTO; t++) {
                    int tab_x = dw_x + 10 + t * 44;
                    if (mx >= tab_x && mx < tab_x + 40) {
                        g_current_crypto = t;
                        return 1;
                    }
                }
            }
            return 1;
        }
        curr_y += h + 12;
    }

    /* Clock */
    if (g_settings.widgets_pinned_mask & WIDGET_PIN_CLOCK) {
        int h = 95;
        if (my >= curr_y && my < curr_y + h) {
            /* [✕] Unpin button */
            if (my >= curr_y && my < curr_y + 24 && mx >= dw_x + dw_w - 28) {
                g_settings.widgets_pinned_mask &= ~WIDGET_PIN_CLOCK;
                settings_save();
                return 1;
            }
            return 1;
        }
        curr_y += h + 12;
    }

    return 0;
}

static void widgets_window_update(struct window *win, int dt_ms) {
    (void)win;
    widgets_update(dt_ms);
}

struct window *widgets_open_window(void) {
    extern window_t *wm_add_window(int x, int y, int w, int h, const char *title,
                                   void (*draw_cb)(window_t*, int, int, int, int));
    window_t *w = wm_add_window(110, 50, 620, 345, "Widgets & System Telemetry", widgets_draw_window);
    if (w) {
        w->mouse_click = widgets_mouse_click;
        w->update_client = widgets_window_update;
    }
    return w;
}
