/* ============================================================================
 * STAX — wm_render.c
 * Window Manager Rendering
 * ============================================================================ */

#include "wm_internal.h"

int bg_color_idx = 0;
uint16_t bg_colors[5] = {
    RGB565_C(52, 73, 94),    /* 0: Deep Slate Blue (Modern sleek default) */
    RGB565_C(22, 101, 89),   /* 1: Emerald Teal */
    RGB565_C(30, 32, 40),    /* 2: Dark Charcoal */
    RGB565_C(114, 30, 45),   /* 3: Crimson Ruby */
    RGB565_C(70, 75, 85)     /* 4: Warm Slate Gray */
};

uint16_t theme_get_primary_accent(void) {
    switch (bg_color_idx) {
        case 0: return rgb565(80, 185, 255);  /* Electric Blue */
        case 1: return rgb565(75, 225, 200);  /* Emerald Teal */
        case 2: return rgb565(210, 220, 235); /* Silver Slate */
        case 3: return rgb565(255, 95, 115);  /* Crimson Coral */
        case 4: return rgb565(190, 200, 215); /* Warm Slate */
        default: return rgb565(80, 185, 255);
    }
}

uint16_t theme_get_secondary_accent(void) {
    switch (bg_color_idx) {
        case 0: return rgb565(140, 205, 255); /* Ice Blue */
        case 1: return rgb565(130, 240, 220); /* Mint Ice */
        case 2: return rgb565(160, 170, 190); /* Muted Silver */
        case 3: return rgb565(255, 160, 175); /* Soft Rose */
        case 4: return rgb565(220, 225, 235); /* Soft White */
        default: return rgb565(140, 205, 255);
    }
}

uint16_t theme_get_desktop_bg(void) {
    if (bg_color_idx >= 0 && bg_color_idx < 5) {
        return bg_colors[bg_color_idx];
    }
    return bg_colors[0];
}

app_icon_t app_icons[NUM_APPS] = {
    {0, 18, 42,  "Browser"},
    {1, 18, 128, "Terminal"},
    {2, 18, 214, "Files"},
    {3, 18, 300, "Notes"},
    {4, 18, 386, "Calculator"},
    {5, 104, 42,  "Sys Info"},
    {6, 104, 128, "Task Mgr"},
    {7, 104, 214, "DOOM"},
    {8, 104, 300, "Settings"}
};

uint16_t *desktop_bg_image = NULL;

const char cursor_bitmap[CURSOR_H][CURSOR_W] = {
    {'X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
    {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' '},
    {'X','.','X',' ',' ',' ',' ',' ',' ',' ',' '},
    {'X','.','.','X',' ',' ',' ',' ',' ',' ',' '},
    {'X','.','.','.','X',' ',' ',' ',' ',' ',' '},
    {'X','.','.','.','.','X',' ',' ',' ',' ',' '},
    {'X','.','.','.','.','.','X',' ',' ',' ',' '},
    {'X','.','.','.','.','.','.','X',' ',' ',' '},
    {'X','.','.','.','.','.','.','.','X',' ',' '},
    {'X','.','.','.','.','.','.','.','.','X',' '},
    {'X','.','.','.','.','X','X','X','X','X','X'},
    {'X','.','.','X','.','.','X',' ',' ',' ',' '},
    {'X','.','X',' ','X','.','.','X',' ',' ',' '},
    {'X','X',' ',' ','X','.','.','X',' ',' ',' '},
    {'X',' ',' ',' ',' ','X','.','.','X',' ',' '},
    {' ',' ',' ',' ',' ',' ','X','X','X','X',' '}
};

#include "font.h"

void draw_text(int x, int y, const char *s, uint16_t color) {
    font_draw_text(x, y, s, color, FONT_STYLE_REGULAR);
}

void draw_window(window_t *win) {
    if (win->state == WM_STATE_HIDDEN || win->state == WM_STATE_MINIMIZED) return;
    
    int wx = win->x;
    int wy = win->y;
    int ww = win->width;
    int wh = win->height;
    
    /* Drop shadow (only for floating windows) */
    if (!win->is_maximized) {
        fb_fill_rounded_rect(wx + 4, wy + 4, ww, wh, 4, rgb565(18, 20, 26));
    }
    
    /* Background */
    fb_fillrect(wx, wy, ww, wh, COL_WIN_BG);
    
    int is_focused = (focused_window == win);
    uint16_t theme_pri = theme_get_primary_accent();
    uint16_t theme_sec = theme_get_secondary_accent();

    /* Borders (Focused window gets dynamic theme accent border) */
    if (is_focused) {
        fb_drawline(wx, wy, wx+ww-1, wy, theme_pri);
        fb_drawline(wx, wy, wx, wy+wh-1, theme_pri);
        fb_drawline(wx+ww-1, wy, wx+ww-1, wy+wh-1, theme_pri);
        fb_drawline(wx, wy+wh-1, wx+ww-1, wy+wh-1, theme_pri);
    } else {
        fb_drawline(wx, wy, wx+ww-1, wy, COL_WIN_BORDER_LIGHT);
        fb_drawline(wx, wy, wx, wy+wh-1, COL_WIN_BORDER_LIGHT);
        fb_drawline(wx+ww-1, wy, wx+ww-1, wy+wh-1, COL_WIN_BORDER_DARK);
        fb_drawline(wx, wy+wh-1, wx+ww-1, wy+wh-1, COL_WIN_BORDER_DARK);
    }
    
    /* Titlebar (Modern Ubuntu Yaru Dark with Accent Highlight) */
    int tbx = wx + BORDER_WIDTH;
    int tby = wy + BORDER_WIDTH;
    int tbw = ww - BORDER_WIDTH*2;
    fb_fillrect(tbx, tby, tbw, TITLEBAR_HEIGHT, is_focused ? rgb565(36, 38, 48) : rgb565(46, 48, 56));
    fb_drawline(tbx, tby, tbx + tbw - 1, tby, is_focused ? theme_pri : rgb565(75, 78, 90));
    fb_drawline(tbx, tby + TITLEBAR_HEIGHT - 1, tbx + tbw - 1, tby + TITLEBAR_HEIGHT - 1, rgb565(22, 24, 30));
    
    /* Modern Ubuntu Window Controls (Right Aligned: Minimize _, Maximize □, Close ✕) */
    int btn_size = 14;
    int btn_y = tby + (TITLEBAR_HEIGHT - btn_size) / 2;
    int close_x = tbx + tbw - 20;
    int max_x   = close_x - 18;
    int min_x   = max_x - 18;

    /* Minimize Button (_) */
    fb_fill_rounded_rect(min_x, btn_y, 14, 14, 3, rgb565(62, 65, 76));
    fb_drawline(min_x + 3, btn_y + 9, min_x + 10, btn_y + 9, COLOR_WHITE);
    fb_drawline(min_x + 3, btn_y + 10, min_x + 10, btn_y + 10, COLOR_WHITE);

    /* Maximize Button (□) */
    fb_fill_rounded_rect(max_x, btn_y, 14, 14, 3, rgb565(62, 65, 76));
    if (win->is_maximized) {
        fb_drawline(max_x + 5, btn_y + 3, max_x + 11, btn_y + 3, COLOR_WHITE);
        fb_drawline(max_x + 11, btn_y + 3, max_x + 11, btn_y + 8, COLOR_WHITE);
        fb_drawline(max_x + 3, btn_y + 5, max_x + 9, btn_y + 5, COLOR_WHITE);
        fb_drawline(max_x + 3, btn_y + 11, max_x + 9, btn_y + 11, COLOR_WHITE);
        fb_drawline(max_x + 3, btn_y + 5, max_x + 3, btn_y + 11, COLOR_WHITE);
        fb_drawline(max_x + 9, btn_y + 5, max_x + 9, btn_y + 11, COLOR_WHITE);
    } else {
        fb_drawline(max_x + 3, btn_y + 3, max_x + 10, btn_y + 3, COLOR_WHITE);
        fb_drawline(max_x + 3, btn_y + 10, max_x + 10, btn_y + 10, COLOR_WHITE);
        fb_drawline(max_x + 3, btn_y + 3, max_x + 3, btn_y + 10, COLOR_WHITE);
        fb_drawline(max_x + 10, btn_y + 3, max_x + 10, btn_y + 10, COLOR_WHITE);
    }

    /* Close Button (✕, Ubuntu Signature Orange) */
    fb_fill_rounded_rect(close_x, btn_y, 14, 14, 3, rgb565(233, 84, 32));
    fb_drawline(close_x + 3, btn_y + 3, close_x + 10, btn_y + 10, COLOR_WHITE);
    fb_drawline(close_x + 4, btn_y + 3, close_x + 11, btn_y + 10, COLOR_WHITE);
    fb_drawline(close_x + 10, btn_y + 3, close_x + 3, btn_y + 10, COLOR_WHITE);
    fb_drawline(close_x + 11, btn_y + 3, close_x + 4, btn_y + 10, COLOR_WHITE);

    /* Title text (Left-aligned Ubuntu style) */
    int max_title_chars = (tbw - 70) / 8;
    if (max_title_chars < 1) max_title_chars = 1;
    char title_buf[32];
    int tl = 0;
    for (; tl < max_title_chars && win->title[tl] && tl < 30; tl++) {
        title_buf[tl] = win->title[tl];
    }
    title_buf[tl] = '\0';
    draw_text(tbx + 10, tby + 2, title_buf, is_focused ? theme_sec : rgb565(195, 200, 210));

    
    /* Client Area */
    int cx = wx + BORDER_WIDTH;
    int cy = wy + BORDER_WIDTH + TITLEBAR_HEIGHT;
    int cw = ww - BORDER_WIDTH*2;
    int ch = wh - BORDER_WIDTH*2 - TITLEBAR_HEIGHT;
    
    fb_fillrect(cx, cy, cw, ch, COLOR_BLACK); /* default black client bg */
    if (win->draw_client) {
        win->draw_client(win, cx, cy, cw, ch);
    }
}

extern uint16_t *bmp_load(const char *filename, int *out_w, int *out_h);
void wm_load_background(const char *filename) {
    if (desktop_bg_image) {
        extern void kfree(void*);
        kfree(desktop_bg_image);
        desktop_bg_image = NULL;
    }
    int w = 0, h = 0;
    desktop_bg_image = bmp_load(filename, &w, &h);
}

#include "icons.h"

void wm_render(void) {
    /* ---- 1. Desktop background (Clean Minimalist Surface) ---- */
    if (desktop_bg_image) {
        extern uint16_t *fb_get_buffer(void);
        uint16_t *fbuf = fb_get_buffer();
        if (fbuf) {
            memcpy(fbuf, desktop_bg_image, fb_width * fb_height * 2);
        }
    } else {
        fb_clear(COL_DESKTOP);
    }
    
    /* ---- 2. Filesystem icons (from SD card) ---- */
    if (!desk_loaded) desk_load_files();

    for (int i = 0; i < desk_count; i++) {
        if (!desk_files[i].valid) continue;
        int ix = desk_files[i].x;
        int iy = desk_files[i].y;
        if (iy + DESK_ICON_H > (int)fb_height) continue;

        icon_draw_desktop_file(ix, iy, desk_files[i].name, desk_files[i].is_dir);

        char lbl[12]; int j;
        for (j=0; j<8 && desk_files[i].name[j]; j++) lbl[j]=desk_files[i].name[j];
        lbl[j]='\0';
        int pill_w = font_get_string_width(lbl, FONT_STYLE_REGULAR) + 12;
        int pill_x = ix + (ICON_W - pill_w) / 2;
        int pill_y = iy + 52;
        fb_fill_rounded_rect(pill_x, pill_y, pill_w, 18, 3, rgb565(20, 22, 28));
        draw_text(pill_x + 6, pill_y + 1, lbl, COLOR_WHITE);
    }

    window_t *arr[32];
    int count = 0;
    window_t *curr = window_list;
    while (curr && count < 32) {
        arr[count++] = curr;
        curr = curr->next;
    }
    for (int i = count - 1; i >= 0; i--) {
        draw_window(arr[i]);
    }
    
    /* 3. Top Navigation Bar (Modern Dark Glassmorphic Style) */
    int ty = 0;
    fb_fillrect(0, ty, fb_width, TASKBAR_HEIGHT, rgb565(26, 28, 36));
    fb_drawline(0, ty + TASKBAR_HEIGHT - 1, fb_width, ty + TASKBAR_HEIGHT - 1, rgb565(48, 52, 65));
    
    /* 1. STAX Logo Button */
    int stax_btn_x = 6;
    int stax_btn_w = 32;
    uint16_t stax_bg = stax_menu_active ? theme_get_primary_accent() : rgb565(38, 42, 54);
    fb_fill_rounded_rect(stax_btn_x, ty + 3, stax_btn_w, 22, 3, stax_bg);

    static uint16_t *stax_logo = NULL;
    static int logo_w = 0, logo_h = 0, logo_attempted = 0;
    if (!stax_logo && !logo_attempted) {
        extern uint16_t *bmp_load(const char *filename, int *out_w, int *out_h);
        stax_logo = bmp_load("BMP/LOGO.BMP", &logo_w, &logo_h);
        logo_attempted = 1;
    }
    if (stax_logo) {
        for (int fy = 0; fy < logo_h; fy++) {
            for (int fx = 0; fx < logo_w; fx++) {
                uint16_t p = stax_logo[fy * logo_w + fx];
                fb_putpixel(stax_btn_x + (stax_btn_w - logo_w)/2 + fx, ty + (TASKBAR_HEIGHT - logo_h) / 2 + fy, p);
            }
        }
    } else {
        draw_text(stax_btn_x + 8, ty + 6, "S", stax_menu_active ? COLOR_WHITE : COLOR_WHITE);
    }

    /* 2. Apps Dropdown Button */
    int app_btn_x = 44;
    int app_btn_w = 52;
    uint16_t app_bg = apps_menu_active ? theme_get_primary_accent() : rgb565(38, 42, 54);
    uint16_t app_fg = apps_menu_active ? COLOR_WHITE : rgb565(220, 225, 240);
    fb_fill_rounded_rect(app_btn_x, ty + 3, app_btn_w, 22, 3, app_bg);
    draw_text(app_btn_x + 10, ty + 6, "Apps", app_fg);

    /* Vertical separator */
    fb_drawline(102, ty + 5, 102, ty + 22, rgb565(48, 52, 65));

    /* Collect only open (non-hidden) windows for navigation tabs */
    window_t *tab_arr[32];
    int tab_count = 0;
    for (int i = 0; i < count; i++) {
        if (arr[i]->state != WM_STATE_HIDDEN) {
            tab_arr[tab_count++] = arr[i];
        }
    }

    /* Open Window Tabs in Navigation Bar */
    int nav_x = 110;
    int max_nav_x = (int)fb_width - 280;
    int avail_w = max_nav_x - nav_x;
    
    if (tab_count > 0 && avail_w > 80) {
        int tab_gap = 4;
        int tab_w = (avail_w - (tab_count - 1) * tab_gap) / tab_count;
        if (tab_w > 130) tab_w = 130;
        
        int visible_tabs = tab_count;
        int overflow_count = 0;
        if (tab_w < 55) {
            tab_w = 55;
            int max_fit = (avail_w - 45) / (tab_w + tab_gap);
            if (max_fit < 1) max_fit = 1;
            visible_tabs = max_fit;
            overflow_count = tab_count - visible_tabs;
        }

        for (int i = 0; i < visible_tabs; i++) {
            window_t *w = tab_arr[i];
            int tx = nav_x + i * (tab_w + tab_gap);
            int is_active = (i == 0 && w->state == WM_STATE_ACTIVE);
            int is_min = (w->state == WM_STATE_MINIMIZED);

            uint16_t bg = is_active ? theme_get_desktop_bg() : (is_min ? rgb565(32, 34, 44) : rgb565(38, 42, 54));
            uint16_t fg = is_active ? COLOR_WHITE : (is_min ? rgb565(130, 135, 145) : rgb565(200, 210, 230));

            fb_fill_rounded_rect(tx, ty + 3, tab_w, 22, 3, bg);
            if (is_active) {
                fb_drawline(tx, ty + 3, tx + tab_w - 1, ty + 3, theme_get_primary_accent());
            }

            /* Small status indicator dot */
            uint16_t dot_col = is_active ? theme_get_primary_accent() : (is_min ? rgb565(100, 105, 120) : rgb565(80, 180, 240));
            fb_fillrect(tx + 6, ty + 12, 4, 4, dot_col);

            char title_trunc[16];
            int ti;
            for (ti = 0; ti < 12 && w->title[ti]; ti++) {
                title_trunc[ti] = w->title[ti];
            }
            title_trunc[ti] = '\0';
            draw_text(tx + 14, ty + 6, title_trunc, fg);
        }

        /* Overflow pill if more tabs exist */
        if (overflow_count > 0) {
            int ox = nav_x + visible_tabs * (tab_w + tab_gap);
            fb_fill_rounded_rect(ox, ty + 3, 38, 22, 3, rgb565(40, 44, 56));

            char ovf_str[8];
            ovf_str[0] = '+';
            if (overflow_count >= 10) {
                ovf_str[1] = '0' + (overflow_count / 10);
                ovf_str[2] = '0' + (overflow_count % 10);
                ovf_str[3] = '\0';
            } else {
                ovf_str[1] = '0' + overflow_count;
                ovf_str[2] = '\0';
            }
            draw_text(ox + 6, ty + 6, ovf_str, rgb565(200, 210, 230));
        }
    }
    
    /* Real-Time Date & Time (IST Mumbai) in Sleek Pill */
    char dt_str[32];
    extern void rtc_format_ist_navbar(char *buf, int max_len);
    rtc_format_ist_navbar(dt_str, sizeof(dt_str));
    int dt_w = font_get_string_width(dt_str, FONT_STYLE_REGULAR) + 12;
    int dt_x = fb_width - dt_w - 8;
    fb_fill_rounded_rect(dt_x, ty + 3, dt_w, 22, 3, rgb565(36, 40, 52));
    draw_text(dt_x + 6, ty + 6, dt_str, COLOR_WHITE);
    
    /* Dynamic Real-Time Memory Usage Pill with Mini Live Chart */
    extern int get_total_memory(void);
    extern int get_free_memory(void);
    uint32_t tot = get_total_memory();
    uint32_t f = get_free_memory();
    uint32_t tot_kb = tot / 1024;
    uint32_t used_kb = (tot >= f) ? (tot - f) / 1024 : 0;
    
    char mem_str[24];
    if (used_kb >= 1024) {
        uint32_t mb_int = used_kb / 1024;
        uint32_t mb_dec = ((used_kb % 1024) * 10) / 1024;
        int mi = 0;
        if (mb_int >= 10) mem_str[mi++] = '0' + (mb_int / 10);
        mem_str[mi++] = '0' + (mb_int % 10);
        mem_str[mi++] = '.';
        mem_str[mi++] = '0' + mb_dec;
        mem_str[mi++] = ' ';
        mem_str[mi++] = 'M';
        mem_str[mi++] = 'B';
        mem_str[mi] = '\0';
    } else {
        int mi = 0;
        char numbuf[12];
        int ni = 0;
        uint32_t temp = used_kb;
        if (temp == 0) numbuf[ni++] = '0';
        else {
            char t2[12]; int ti = 0;
            while (temp) { t2[ti++] = '0' + (temp % 10); temp /= 10; }
            while (ti > 0) numbuf[ni++] = t2[--ti];
        }
        for (int k = 0; k < ni; k++) mem_str[mi++] = numbuf[k];
        mem_str[mi++] = ' ';
        mem_str[mi++] = 'K';
        mem_str[mi++] = 'B';
        mem_str[mi] = '\0';
    }

    /* History sparkline ring buffer */
    static uint8_t s_mem_history[10] = {14, 15, 14, 16, 17, 16, 18, 17, 19, 18};
    static uint32_t s_last_sample_tick = 0;
    extern volatile unsigned int tick_count;
    uint32_t cur_ticks = tick_count;
    if (cur_ticks - s_last_sample_tick >= 1000) {
        s_last_sample_tick = cur_ticks;
        uint8_t cur_pct = (tot_kb > 0) ? (uint8_t)(((uint64_t)used_kb * 100) / tot_kb) : 0;
        if (cur_pct == 0 && used_kb > 0) cur_pct = 5;
        for (int hi = 0; hi < 9; hi++) s_mem_history[hi] = s_mem_history[hi + 1];
        s_mem_history[9] = cur_pct;
    }

    int chart_w = 10 * 3; /* 10 bars * (2px width + 1px gap) = 30px */
    int text_w = font_get_string_width(mem_str, FONT_STYLE_REGULAR);
    int mem_w = chart_w + text_w + 16;
    int mem_x = dt_x - mem_w - 6;

    fb_fill_rounded_rect(mem_x, ty + 3, mem_w, 22, 3, rgb565(36, 40, 52));

    /* Render mini real-time sparkline chart */
    int bar_base_y = ty + 18;
    for (int bi = 0; bi < 10; bi++) {
        int bx = mem_x + 6 + bi * 3;
        int val = s_mem_history[bi];
        int bh = (val * 13) / 100;
        if (bh < 2) bh = 2;
        if (bh > 13) bh = 13;
        uint16_t bcol = (val > 80) ? rgb565(230, 60, 60) :
                        (val > 50) ? rgb565(240, 180, 40) :
                        theme_get_primary_accent();
        fb_fillrect(bx, bar_base_y - bh, 2, bh, bcol);
    }

    /* Render live RAM text */
    draw_text(mem_x + 6 + chart_w + 4, ty + 6, mem_str, COLOR_WHITE);
    
    /* Desktop Context Menu (Clean & Simple) */
    if (ctx_menu.active) {
        int cm_x = ctx_menu.x;
        int cm_y = ctx_menu.y;
        int cm_w = 160;
        int cm_h = 112;

        /* Drop shadow */
        fb_fillrect(cm_x + 3, cm_y + 3, cm_w, cm_h, rgb565(15, 17, 22));

        /* Background (Ubuntu Dark Slate Theme) */
        fb_fillrect(cm_x, cm_y, cm_w, cm_h, rgb565(36, 38, 46));

        /* Border (Top border has dynamic theme accent) */
        fb_drawline(cm_x, cm_y, cm_x + cm_w - 1, cm_y, theme_get_primary_accent());
        fb_drawline(cm_x, cm_y, cm_x, cm_y + cm_h - 1, rgb565(70, 75, 90));
        fb_drawline(cm_x + cm_w - 1, cm_y, cm_x + cm_w - 1, cm_y + cm_h - 1, rgb565(20, 22, 28));
        fb_drawline(cm_x, cm_y + cm_h - 1, cm_x + cm_w - 1, cm_y + cm_h - 1, rgb565(20, 22, 28));

        /* Item 0: Open Terminal */
        draw_text(cm_x + 12, cm_y + 6, "Open Terminal", COLOR_WHITE);
        fb_drawline(cm_x + 8, cm_y + 27, cm_x + cm_w - 8, cm_y + 27, rgb565(50, 54, 65));

        /* Item 1: New Document */
        draw_text(cm_x + 12, cm_y + 34, "New Document", COLOR_WHITE);
        fb_drawline(cm_x + 8, cm_y + 55, cm_x + cm_w - 8, cm_y + 55, rgb565(50, 54, 65));

        /* Item 2: Refresh Desktop */
        draw_text(cm_x + 12, cm_y + 62, "Refresh Desktop", theme_get_secondary_accent());
        fb_drawline(cm_x + 8, cm_y + 83, cm_x + cm_w - 8, cm_y + 83, rgb565(50, 54, 65));

        /* Item 3: Settings */
        draw_text(cm_x + 12, cm_y + 90, "Settings...", COLOR_WHITE);
    }

    /* 1. STAX System Dropdown Menu (Under Logo) */
    if (stax_menu_active) {
        int sm_x = 0;
        int sm_y = TASKBAR_HEIGHT;
        int sm_w = 190;
        int sm_h = 195;

        /* Drop shadow */
        fb_fillrect(sm_x + 3, sm_y + 3, sm_w, sm_h, rgb565(20, 22, 28));

        /* Menu card */
        fb_fillrect(sm_x, sm_y, sm_w, sm_h, rgb565(36, 38, 46));
        fb_drawline(sm_x, sm_y, sm_x + sm_w - 1, sm_y, theme_get_primary_accent());
        fb_drawline(sm_x, sm_y, sm_x, sm_y + sm_h - 1, rgb565(70, 75, 90));
        fb_drawline(sm_x + sm_w - 1, sm_y, sm_x + sm_w - 1, sm_y + sm_h - 1, rgb565(20, 22, 28));
        fb_drawline(sm_x, sm_y + sm_h - 1, sm_x + sm_w - 1, sm_y + sm_h - 1, rgb565(20, 22, 28));

        /* Menu Items */
        draw_text(sm_x + 12, sm_y + 8, "About", COLOR_WHITE);
        fb_drawline(sm_x + 6, sm_y + 30, sm_x + sm_w - 6, sm_y + 30, rgb565(55, 58, 70));

        draw_text(sm_x + 12, sm_y + 38, "System Settings", COLOR_WHITE);
        draw_text(sm_x + 12, sm_y + 68, "Task Manager", COLOR_WHITE);
        draw_text(sm_x + 12, sm_y + 98, "System Info", COLOR_WHITE);

        fb_drawline(sm_x + 6, sm_y + 126, sm_x + sm_w - 6, sm_y + 126, rgb565(55, 58, 70));

        draw_text(sm_x + 12, sm_y + 136, "Reboot System", rgb565(240, 80, 80));
        draw_text(sm_x + 12, sm_y + 166, "Force Quit...", rgb565(160, 165, 180));
    }
    
    /* 2. Applications Dropdown Menu (Under Apps button) */
    if (apps_menu_active) {
        int app_x = 44;
        int app_y = TASKBAR_HEIGHT;
        int app_w = 185;
        int app_h = 265;
        int item_h = 28;
        
        /* Drop shadow */
        fb_fillrect(app_x + 3, app_y + 3, app_w, app_h, rgb565(20, 22, 28));

        /* Menu card */
        fb_fillrect(app_x, app_y, app_w, app_h, rgb565(36, 38, 46));
        fb_drawline(app_x, app_y, app_x + app_w - 1, app_y, theme_get_primary_accent());
        fb_drawline(app_x, app_y, app_x, app_y + app_h - 1, rgb565(70, 75, 90));
        fb_drawline(app_x + app_w - 1, app_y, app_x + app_w - 1, app_y + app_h - 1, rgb565(20, 22, 28));
        fb_drawline(app_x, app_y + app_h - 1, app_x + app_w - 1, app_y + app_h - 1, rgb565(20, 22, 28));

        const char *app_list[9] = {
            "Web Browser",
            "Terminal",
            "File Manager",
            "Text Editor",
            "Calculator",
            "System Info",
            "Task Manager",
            "DOOM Game",
            "Settings"
        };

        for (int i = 0; i < 9; i++) {
            int iy = app_y + 6 + i * item_h;
            if (i == 5 || i == 7) {
                fb_drawline(app_x + 6, iy - 2, app_x + app_w - 6, iy - 2, rgb565(55, 58, 70));
            }
            draw_text(app_x + 14, iy + 4, app_list[i], COLOR_WHITE);
        }
    }
    
    /* 4. Mouse Cursor */
    int cx = mouse_x;
    int cy = mouse_y;
    for (int r = 0; r < CURSOR_H; r++) {
        for (int c = 0; c < CURSOR_W; c++) {
            char pixel = cursor_bitmap[r][c];
            if (pixel == 'X') {
                fb_putpixel(cx + c, cy + r, COLOR_BLACK);
            } else if (pixel == '.') {
                fb_putpixel(cx + c, cy + r, COLOR_WHITE);
            }
        }
    }
    
    /* 5. Window Switcher Overlay (Ctrl+Tab) — drawn on top of everything */
    switcher_draw();

    /* 6. Swap */
    fb_swap();
}

/* ============================================================================
 * Ctrl+Tab Window Switcher Overlay (Clean, Rectangular, Minimalist Tabs)
 * ============================================================================ */

static uint16_t sw_dim(uint16_t c, int alpha) {
    int r = ((c >> 11) & 0x1f) * alpha / 255;
    int g = ((c >>  5) & 0x3f) * alpha / 255;
    int b = ( c        & 0x1f) * alpha / 255;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static void sw_dim_rect(int x, int y, int w, int h, int alpha) {
    uint16_t *fb = fb_get_buffer();
    if (!fb) return;
    int fw2 = (int)fb_width;
    int fh2 = (int)fb_height;
    for (int row = y; row < y + h; row++) {
        if (row < 0 || row >= fh2) continue;
        for (int col = x; col < x + w; col++) {
            if (col < 0 || col >= fw2) continue;
            fb[row * fw2 + col] = sw_dim(fb[row * fw2 + col], alpha);
        }
    }
}

void switcher_draw(void) {
    if (!g_switcher.active || g_switcher.count < 1) return;

    uint16_t *fb = fb_get_buffer();
    if (!fb) return;
    int fw2 = (int)fb_width;
    int fh2 = (int)fb_height;

    /* ---- 1. Subtle Screen Dim ---- */
    sw_dim_rect(0, 0, fw2, fh2, 130);

    /* ---- 2. 2D Grid Geometry (Columns & Rows) ---- */
    int count = g_switcher.count;
    int cols = count;
    if (cols > 4) cols = 4;
    if (count == 5 || count == 6) cols = 3;
    int rows = (count + cols - 1) / cols;

    int tab_w   = 136;
    int tab_h   = 32;
    int gap_x   = 6;
    int gap_y   = 6;
    int pad_x   = 10;
    int pad_y   = 10;

    int panel_w = pad_x * 2 + cols * tab_w + (cols - 1) * gap_x;
    int panel_h = pad_y * 2 + rows * tab_h + (rows - 1) * gap_y;

    if (panel_w > fw2 - 32) {
        tab_w = (fw2 - 32 - pad_x * 2 - (cols - 1) * gap_x) / cols;
        if (tab_w < 70) tab_w = 70;
        panel_w = pad_x * 2 + cols * tab_w + (cols - 1) * gap_x;
    }

    int panel_x = (fw2 - panel_w) / 2;
    int panel_y = (fh2 - panel_h) / 2;

    /* ---- 3. Modal Shell (Rectangular Dark Surface) ---- */
    /* Drop shadow */
    fb_fillrect(panel_x + 3, panel_y + 3, panel_w, panel_h, rgb565(16, 18, 24));

    /* Panel background */
    fb_fillrect(panel_x, panel_y, panel_w, panel_h, rgb565(36, 38, 48));

    /* Borders */
    uint16_t accent = theme_get_primary_accent();
    fb_drawline(panel_x,               panel_y,               panel_x + panel_w - 1, panel_y,               accent);
    fb_drawline(panel_x,               panel_y + panel_h - 1, panel_x + panel_w - 1, panel_y + panel_h - 1, rgb565(20, 22, 28));
    fb_drawline(panel_x,               panel_y,               panel_x,               panel_y + panel_h - 1, rgb565(60, 64, 76));
    fb_drawline(panel_x + panel_w - 1, panel_y,               panel_x + panel_w - 1, panel_y + panel_h - 1, rgb565(20, 22, 28));

    /* ---- 4. Smooth 2D Sliding Selection Tab ---- */
    int cx0 = panel_x + pad_x;
    int cy0 = panel_y + pad_y;

    int sel_c = g_switcher.sel % cols;
    int sel_r = g_switcher.sel / cols;

    int target_x = cx0 + sel_c * (tab_w + gap_x);
    int target_y = cy0 + sel_r * (tab_h + gap_y);
    int target_x_fp = target_x * 256;
    int target_y_fp = target_y * 256;

    if (!g_switcher.anim_inited) {
        g_switcher.anim_x_fp = target_x_fp;
        g_switcher.anim_y_fp = target_y_fp;
        g_switcher.anim_inited = 1;
    } else {
        int diff_x = target_x_fp - g_switcher.anim_x_fp;
        int diff_y = target_y_fp - g_switcher.anim_y_fp;
        g_switcher.anim_x_fp += (diff_x * 45) / 100;
        g_switcher.anim_y_fp += (diff_y * 45) / 100;
    }

    int anim_sel_x = g_switcher.anim_x_fp / 256;
    int anim_sel_y = g_switcher.anim_y_fp / 256;

    /* Active Highlight Tab */
    fb_fillrect(anim_sel_x, anim_sel_y, tab_w, tab_h, rgb565(54, 58, 74));
    fb_fillrect(anim_sel_x, anim_sel_y + tab_h - 2, tab_w, 2, accent);
    fb_drawline(anim_sel_x, anim_sel_y, anim_sel_x + tab_w - 1, anim_sel_y, rgb565(75, 80, 100));

    /* ---- 5. Render Clean Window Tabs in 2D Grid ---- */
    for (int i = 0; i < count; i++) {
        window_t *win = g_switcher.wins[i];
        int r = i / cols;
        int c = i % cols;
        int cx = cx0 + c * (tab_w + gap_x);
        int cy = cy0 + r * (tab_h + gap_y);
        int is_sel = (i == g_switcher.sel);

        /* Unselected Tab Background */
        if (!is_sel) {
            fb_fillrect(cx, cy, tab_w, tab_h, rgb565(44, 46, 56));
            fb_drawline(cx, cy, cx + tab_w - 1, cy, rgb565(56, 59, 70));
            fb_drawline(cx, cy + tab_h - 1, cx + tab_w - 1, cy + tab_h - 1, rgb565(28, 30, 38));
        }

        /* Format Window Title */
        char title_buf[24];
        int max_chars = (tab_w - 16) / 8;
        if (max_chars > 20) max_chars = 20;
        if (max_chars < 4) max_chars = 4;

        int ti = 0;
        while (ti < max_chars && win->title[ti]) {
            title_buf[ti] = win->title[ti];
            ti++;
        }
        title_buf[ti] = '\0';

        /* If minimized, append subtle marker if space permits */
        if (win->state == WM_STATE_MINIMIZED && ti + 5 < (int)sizeof(title_buf) && ti + 5 <= max_chars) {
            const char *min_s = " (min)";
            for (int k = 0; min_s[k]; k++) title_buf[ti++] = min_s[k];
            title_buf[ti] = '\0';
        }

        /* Center Text inside Tab */
        int tw = font_get_string_width(title_buf, FONT_STYLE_REGULAR);
        int tx = cx + (tab_w - tw) / 2;
        int ty = cy + (tab_h - 14) / 2;

        uint16_t text_col = is_sel ? COLOR_WHITE : (win->state == WM_STATE_MINIMIZED ? rgb565(150, 155, 170) : rgb565(200, 205, 220));
        font_draw_text(tx, ty, title_buf, text_col, FONT_STYLE_REGULAR);
    }
}


