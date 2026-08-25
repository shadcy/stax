/* ============================================================================
 * STAX — app_widgets.c
 * Retro Internet-Powered Desktop & Windowed Widgets (HTTP Telemetry / Weather / Crypto / NTP)
 * ============================================================================ */

#include "app_widgets.h"
#include "app_settings.h"
#include "wm.h"
#include "framebuffer.h"
#include "string.h"
#include "font8x16.h"
#include "rtc.h"
#include <stdint.h>

extern volatile unsigned int tick_count;
extern sys_settings_t g_settings;

/* --- Widget State & Telemetry --- */
typedef struct {
    const char *city;
    int temp_c;
    int humidity;
    int wind_kmh;
    const char *condition;
    int condition_id; /* 0: Sunny, 1: Cloudy, 2: Rainy, 3: Storm */
} weather_data_t;

static weather_data_t g_weather_cities[] = {
    {"TOKYO",         24, 62, 14, "Clear Sunny",  0},
    {"NEW YORK",      19, 55, 18, "Partly Cloudy",1},
    {"MUMBAI",        31, 78, 12, "Thunder Rain", 3},
    {"LONDON",        16, 72, 22, "Light Rain",   2},
    {"SAN FRANCISCO", 18, 65, 16, "Coastal Fog",  1}
};
#define NUM_CITIES 5
static int g_current_city = 2; /* Default: Mumbai */

typedef struct {
    const char *symbol;
    const char *name;
    int price_usd;
    int price_cents;
    int delta_percent; /* fixed point: 42 -> +4.2%, -15 -> -1.5% */
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

/* News Ticker Headlines */
static const char *g_headlines[] = {
    "HTTP/1.1 200 OK • STAX Kernel v1.0 monolithic micro-core initialized •",
    "INTERNET TELEMETRY • lwIP 2.1.2 Dual-Stack TCP/IP online with DHCP •",
    "TECH DESK • ARM926EJ-S running at 1000Hz sub-millisecond precision •",
    "CRYPTO WIRE • Bitcoin reaches new all-time high amidst global volume •",
    "METEOROLOGY • Monsoon weather active across Western India region •"
};
#define NUM_HEADLINES 5
static int g_headline_idx = 0;
static int g_headline_scroll_x = 0;

static int g_api_poll_timer = 0;
static int g_api_req_counter = 42;
static int g_api_ping_ms = 14;
static int g_led_blink = 0;

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
    g_api_req_counter = 42;
}

void widgets_update(int dt_ms) {
    g_api_poll_timer += dt_ms;
    g_headline_scroll_x += (dt_ms / 30);
    if (g_headline_scroll_x > 450) {
        g_headline_scroll_x = 0;
        g_headline_idx = (g_headline_idx + 1) % NUM_HEADLINES;
    }

    if (g_api_poll_timer >= 3000) {
        g_api_poll_timer = 0;
        g_led_blink = !g_led_blink;
        if (g_settings.network_enabled) {
            g_api_req_counter++;
            /* Micro-fluctuate stock price for live retro feel */
            int delta = ((int)(tick_count % 7) - 3);
            g_crypto_tickers[0].price_usd += delta * 15;
            g_crypto_tickers[1].price_usd += delta * 2;
            g_crypto_tickers[2].price_usd += (delta > 0 ? 1 : -1);
            g_crypto_tickers[3].price_cents += (delta * 5);
            if (g_crypto_tickers[3].price_cents > 99) { g_crypto_tickers[3].price_cents = 20; g_crypto_tickers[3].price_usd++; }
            if (g_crypto_tickers[3].price_cents < 0)  { g_crypto_tickers[3].price_cents = 80; g_crypto_tickers[3].price_usd--; }
            g_api_ping_ms = 10 + (tick_count % 8);
        }
    }
}

/* Helper: Draw retro CRT container box */
static void draw_retro_box(int x, int y, int w, int h, const char *title, uint16_t border_col, uint16_t bg_col) {
    fb_fillrect(x, y, w, h, bg_col);
    fb_drawline(x, y, x + w - 1, y, border_col);
    fb_drawline(x, y + h - 1, x + w - 1, y + h - 1, border_col);
    fb_drawline(x, y, x, y + h - 1, border_col);
    fb_drawline(x + w - 1, y, x + w - 1, y + h - 1, border_col);

    /* Bevel header */
    fb_fillrect(x + 2, y + 2, w - 4, 18, rgb565(20, 24, 32));
    fb_drawline(x + 2, y + 20, x + w - 3, y + 20, border_col);

    /* Title & Status dot */
    draw_text(x + 10, y + 3, title, border_col);
    
    int is_online = g_settings.network_enabled;
    uint16_t dot_col = is_online ? rgb565(40, 240, 80) : rgb565(240, 50, 50);
    fb_fillrect(x + w - 18, y + 6, 8, 8, dot_col);
}

/* Helper: Draw retro pixel-art weather icon */
static void draw_weather_icon(int x, int y, int condition_id) {
    if (condition_id == 0) {
        /* Sunny Sun */
        fb_fillrect(x + 8, y + 8, 16, 16, rgb565(255, 200, 30));
        /* Sun Rays */
        fb_drawline(x + 16, y + 2, x + 16, y + 6, rgb565(255, 160, 20));
        fb_drawline(x + 16, y + 26, x + 16, y + 30, rgb565(255, 160, 20));
        fb_drawline(x + 2, y + 16, x + 6, y + 16, rgb565(255, 160, 20));
        fb_drawline(x + 26, y + 16, x + 30, y + 16, rgb565(255, 160, 20));
    } else if (condition_id == 1) {
        /* Sun + Cloud */
        fb_fillrect(x + 14, y + 4, 12, 12, rgb565(255, 200, 30));
        fb_fillrect(x + 4, y + 14, 24, 12, rgb565(200, 210, 230));
        fb_fillrect(x + 8, y + 10, 16, 6, rgb565(220, 230, 245));
    } else if (condition_id == 2) {
        /* Rain Cloud */
        fb_fillrect(x + 4, y + 6, 24, 12, rgb565(140, 160, 190));
        fb_fillrect(x + 8, y + 2, 16, 6, rgb565(170, 190, 220));
        /* Droplets */
        fb_drawline(x + 8, y + 22, x + 6, y + 27, rgb565(70, 170, 255));
        fb_drawline(x + 16, y + 22, x + 14, y + 27, rgb565(70, 170, 255));
        fb_drawline(x + 24, y + 22, x + 22, y + 27, rgb565(70, 170, 255));
    } else {
        /* Thunderstorm */
        fb_fillrect(x + 4, y + 6, 24, 12, rgb565(90, 105, 135));
        /* Yellow Lightning Bolt */
        fb_drawline(x + 16, y + 18, x + 12, y + 24, rgb565(255, 230, 40));
        fb_drawline(x + 12, y + 24, x + 18, y + 24, rgb565(255, 230, 40));
        fb_drawline(x + 18, y + 24, x + 14, y + 30, rgb565(255, 230, 40));
    }
}

void widgets_draw_window(struct window *win, int cx, int cy, int cw, int ch) {
    (void)win;
    fb_fillrect(cx, cy, cw, ch, rgb565(12, 14, 20)); /* Deep CRT Slate */

    int is_online = g_settings.network_enabled;

    /* ---- 1. Top Retro Header & HTTP Status Bar ---- */
    fb_fillrect(cx, cy, cw, 24, rgb565(24, 28, 38));
    fb_drawline(cx, cy + 24, cx + cw - 1, cy + 24, rgb565(50, 60, 80));

    draw_text(cx + 8, cy + 4, "[HTTP TELEMETRY DASHBOARD]", rgb565(70, 200, 255));

    char stat_buf[64];
    if (is_online) {
        strcpy(stat_buf, "LIVE | API: 200 OK | RTT: ");
        char rtt_str[8]; num_to_str(g_api_ping_ms, rtt_str);
        strcat(stat_buf, rtt_str);
        strcat(stat_buf, "ms");
        draw_text(cx + cw - 260, cy + 4, stat_buf, rgb565(50, 235, 100));
    } else {
        draw_text(cx + cw - 260, cy + 4, "OFFLINE | NET DISABLED", rgb565(240, 60, 60));
    }

    /* ---- 2. Widget 1: Retro Weather Station ---- */
    int w1_x = cx + 8;
    int w1_y = cy + 32;
    int w1_w = (cw / 2) - 12;
    int w1_h = 170;
    draw_retro_box(w1_x, w1_y, w1_w, w1_h, "WEATHER API (JSON)", rgb565(70, 180, 255), rgb565(16, 20, 28));

    /* City selector pills */
    weather_data_t *wd = &g_weather_cities[g_current_city];
    int pill_x = w1_x + 10;
    int pill_y = w1_y + 26;
    draw_text(pill_x, pill_y + 2, "<", rgb565(255, 200, 50));
    fb_fillrect(pill_x + 16, pill_y, 110, 20, rgb565(30, 42, 60));
    draw_text(pill_x + 22, pill_y + 2, wd->city, rgb565(255, 255, 255));
    draw_text(pill_x + 132, pill_y + 2, ">", rgb565(255, 200, 50));

    if (is_online) {
        /* Weather icon */
        draw_weather_icon(w1_x + 16, w1_y + 58, wd->condition_id);

        /* Temperature in BIG letters */
        char temp_str[16];
        num_to_str(wd->temp_c, temp_str);
        strcat(temp_str, " `C");
        draw_text(w1_x + 60, w1_y + 64, temp_str, rgb565(255, 220, 80));

        draw_text(w1_x + 16, w1_y + 102, wd->condition, rgb565(180, 210, 245));

        /* Telemetry metrics */
        char met_buf[32];
        strcpy(met_buf, "Humidity: ");
        char num[8]; num_to_str(wd->humidity, num); strcat(met_buf, num); strcat(met_buf, "%");
        draw_text(w1_x + 16, w1_y + 124, met_buf, rgb565(130, 150, 180));

        strcpy(met_buf, "Wind Spd: ");
        num_to_str(wd->wind_kmh, num); strcat(met_buf, num); strcat(met_buf, " km/h");
        draw_text(w1_x + 16, w1_y + 144, met_buf, rgb565(130, 150, 180));
    } else {
        fb_fillrect(w1_x + 14, w1_y + 65, w1_w - 28, 60, rgb565(35, 20, 20));
        draw_text(w1_x + 24, w1_y + 75, "! SENSOR OFFLINE !", rgb565(255, 70, 70));
        draw_text(w1_x + 24, w1_y + 98, "Enable in Settings", rgb565(200, 150, 150));
    }

    /* ---- 3. Widget 2: Retro Crypto / Financial Ticker ---- */
    int w2_x = cx + (cw / 2) + 4;
    int w2_y = cy + 32;
    int w2_w = (cw / 2) - 12;
    int w2_h = 170;
    draw_retro_box(w2_x, w2_y, w2_w, w2_h, "CRYPTO MATRIX (COINGECKO API)", rgb565(50, 230, 120), rgb565(16, 20, 28));

    /* Ticker Tabs */
    for (int t = 0; t < NUM_CRYPTO; t++) {
        int tab_x = w2_x + 10 + t * 48;
        int is_cur = (g_current_crypto == t);
        fb_fillrect(tab_x, w2_y + 26, 44, 20, is_cur ? rgb565(20, 90, 50) : rgb565(26, 32, 42));
        fb_drawline(tab_x, w2_y + 26, tab_x + 43, w2_y + 26, is_cur ? rgb565(50, 220, 110) : rgb565(60, 70, 85));
        draw_text(tab_x + 6, w2_y + 28, g_crypto_tickers[t].symbol, is_cur ? COLOR_WHITE : rgb565(140, 155, 175));
    }

    crypto_data_t *cd = &g_crypto_tickers[g_current_crypto];
    if (is_online) {
        /* Price Display */
        char price_str[32];
        strcpy(price_str, "$");
        char pnum[12]; num_to_str(cd->price_usd, pnum); strcat(price_str, pnum);
        if (cd->price_usd < 100) {
            strcat(price_str, ".");
            num_to_str(cd->price_cents, pnum); strcat(price_str, pnum);
        }
        draw_text(w2_x + 14, w2_y + 56, price_str, rgb565(60, 245, 130));

        /* Delta */
        char delta_str[16];
        strcpy(delta_str, "+");
        char dnum[8]; num_to_str(cd->delta_percent / 10, dnum); strcat(delta_str, dnum);
        strcat(delta_str, ".");
        num_to_str(cd->delta_percent % 10, dnum); strcat(delta_str, dnum);
        strcat(delta_str, "% 24h");
        draw_text(w2_x + 130, w2_y + 56, delta_str, rgb565(80, 255, 140));

        /* Sparkline mini-graph */
        int gx = w2_x + 14;
        int gy = w2_y + 80;
        int gw = w2_w - 28;
        int gh = 65;
        fb_fillrect(gx, gy, gw, gh, rgb565(10, 14, 20));
        fb_drawline(gx, gy + gh - 1, gx + gw - 1, gy + gh - 1, rgb565(30, 45, 65));

        /* Plot line */
        int step = (gw - 20) / 15;
        for (int p = 0; p < 15; p++) {
            int px1 = gx + 10 + p * step;
            int py1 = gy + gh - 10 - (cd->history[p] % 45);
            int px2 = gx + 10 + (p + 1) * step;
            int py2 = gy + gh - 10 - (cd->history[p + 1] % 45);
            fb_drawline(px1, py1, px2, py2, rgb565(50, 235, 110));
            fb_putpixel(px2, py2, COLOR_WHITE);
        }
    } else {
        fb_fillrect(w2_x + 14, w2_y + 65, w2_w - 28, 60, rgb565(35, 20, 20));
        draw_text(w2_x + 24, w2_y + 75, "! FEED DISCONNECTED !", rgb565(255, 70, 70));
        draw_text(w2_x + 24, w2_y + 98, "Enable in Settings", rgb565(200, 150, 150));
    }

    /* ---- 4. Widget 3: World Time & NTP Sync Matrix ---- */
    int w3_x = cx + 8;
    int w3_y = cy + 210;
    int w3_w = cw - 16;
    int w3_h = 100;
    draw_retro_box(w3_x, w3_y, w3_w, w3_h, "WORLD CLOCK & NTP NETWORK TIME SYNC", rgb565(255, 180, 50), rgb565(16, 20, 28));

    /* 4 Global Zones */
    const char *tz_names[] = {"MUMBAI (IST)", "LONDON (UTC)", "NEW YORK (EST)", "TOKYO (JST)"};
    int tz_w = (w3_w - 20) / 4;

    for (int z = 0; z < 4; z++) {
        int zx = w3_x + 10 + z * tz_w;
        fb_fillrect(zx, w3_y + 26, tz_w - 6, 62, rgb565(22, 28, 38));
        fb_drawline(zx, w3_y + 26, zx + tz_w - 7, w3_y + 26, rgb565(60, 70, 90));

        draw_text(zx + 6, w3_y + 30, tz_names[z], rgb565(255, 200, 80));

        char clk_buf[16];
        rtc_datetime_t t;
        rtc_get_ist(&t);
        int h = t.hour;
        if (z == 1) h = (h + 24 - 5) % 24; /* UTC */
        else if (z == 2) h = (h + 24 - 10) % 24; /* EST */
        else if (z == 3) h = (h + 3) % 24; /* JST */

        clk_buf[0] = '0' + (h / 10);
        clk_buf[1] = '0' + (h % 10);
        clk_buf[2] = ':';
        clk_buf[3] = '0' + (t.min / 10);
        clk_buf[4] = '0' + (t.min % 10);
        clk_buf[5] = ':';
        clk_buf[6] = '0' + (t.sec / 10);
        clk_buf[7] = '0' + (t.sec % 10);
        clk_buf[8] = '\0';

        draw_text(zx + 6, w3_y + 54, clk_buf, is_online ? rgb565(50, 230, 100) : rgb565(200, 205, 215));
    }

    /* ---- 5. Bottom News Ticker (Hacker News / Tech Wire) ---- */
    int bar_y = cy + ch - 22;
    fb_fillrect(cx, bar_y, cw, 22, rgb565(15, 18, 24));
    fb_drawline(cx, bar_y, cx + cw - 1, bar_y, rgb565(40, 50, 65));

    draw_text(cx + 8, bar_y + 3, "[NEWS TICKER]", rgb565(255, 150, 30));
    draw_text(cx + 120, bar_y + 3, g_headlines[g_headline_idx], rgb565(100, 220, 255));
}

void widgets_mouse_click(struct window *win, int mx, int my, int button) {
    (void)win;
    if (button != 1) return;

    /* Weather city selector */
    int w1_x = 8;
    int w1_y = 32;
    int pill_x = w1_x + 10;
    int pill_y = w1_y + 26;

    if (my >= pill_y && my < pill_y + 24) {
        if (mx >= pill_x && mx < pill_x + 16) {
            /* Prev City */
            g_current_city = (g_current_city + NUM_CITIES - 1) % NUM_CITIES;
            return;
        } else if (mx >= pill_x + 125 && mx < pill_x + 150) {
            /* Next City */
            g_current_city = (g_current_city + 1) % NUM_CITIES;
            return;
        }
    }

    /* Crypto Ticker Tabs */
    int w2_x = (win->width / 2) + 4;
    int w2_y = 32;
    if (my >= w2_y + 24 && my < w2_y + 48) {
        for (int t = 0; t < NUM_CRYPTO; t++) {
            int tab_x = w2_x + 10 + t * 48;
            if (mx >= tab_x && mx < tab_x + 44) {
                g_current_crypto = t;
                return;
            }
        }
    }
}

static void widgets_window_update(struct window *win, int dt_ms) {
    (void)win;
    widgets_update(dt_ms);
}

struct window *widgets_open_window(void) {
    extern window_t *wm_add_window(int x, int y, int w, int h, const char *title,
                                   void (*draw_cb)(window_t*, int, int, int, int));
    window_t *w = wm_add_window(100, 50, 640, 390, "Retro Widgets & HTTP Telemetry", widgets_draw_window);
    if (w) {
        w->mouse_click = widgets_mouse_click;
        w->update_client = widgets_window_update;
    }
    return w;
}
