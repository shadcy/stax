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

void draw_text(int x, int y, const char *s, uint16_t color) {
    /* extremely simple unscaled 8x16 font rendering for WM strings */
    extern const unsigned char font8x16_data[256][16];
    while (*s) {
        unsigned char c = *s++;
        for (int r = 0; r < 16; r++) {
            unsigned char bits = font8x16_data[c][r];
            for (int b = 0; b < 8; b++) {
                if (bits & (0x80 >> b)) {
                    fb_putpixel(x + b, y + r, color);
                }
            }
        }
        x += 8;
    }
}

void draw_window(window_t *win) {
    if (win->state == WM_STATE_HIDDEN || win->state == WM_STATE_MINIMIZED) return;
    
    int wx = win->x;
    int wy = win->y;
    int ww = win->width;
    int wh = win->height;
    
    /* Drop shadow */
    fb_fillrect(wx + 4, wy + 4, ww, wh, rgb565(32, 32, 32));
    
    /* Background */
    fb_fillrect(wx, wy, ww, wh, COL_WIN_BG);
    
    /* Borders */
    fb_drawline(wx, wy, wx+ww-1, wy, COL_WIN_BORDER_LIGHT);
    fb_drawline(wx, wy, wx, wy+wh-1, COL_WIN_BORDER_LIGHT);
    fb_drawline(wx+ww-1, wy, wx+ww-1, wy+wh-1, COL_WIN_BORDER_DARK);
    fb_drawline(wx, wy+wh-1, wx+ww-1, wy+wh-1, COL_WIN_BORDER_DARK);
    
    /* Titlebar (Ubuntu Yaru Dark / Aubergine Theme) */
    int tbx = wx + BORDER_WIDTH;
    int tby = wy + BORDER_WIDTH;
    int tbw = ww - BORDER_WIDTH*2;
    fb_fillrect(tbx, tby, tbw, TITLEBAR_HEIGHT, rgb565(44, 44, 44));
    fb_drawline(tbx, tby, tbx + tbw - 1, tby, rgb565(68, 68, 68));
    fb_drawline(tbx, tby + TITLEBAR_HEIGHT - 1, tbx + tbw - 1, tby + TITLEBAR_HEIGHT - 1, rgb565(25, 25, 25));
    
    /* Ubuntu Window Controls (Right Aligned: Minimize _, Maximize □, Close ✕) */
    int btn_size = 14;
    int btn_y = tby + (TITLEBAR_HEIGHT - btn_size) / 2;
    int close_x = tbx + tbw - 20;
    int max_x   = close_x - 18;
    int min_x   = max_x - 18;

    /* Helper: Draw rounded button background */
    #define DRAW_UBUNTU_BTN(bx, by, col) \
        fb_fillrect((bx)+2, (by), 10, 14, col); \
        fb_fillrect((bx), (by)+2, 14, 10, col); \
        fb_fillrect((bx)+1, (by)+1, 12, 12, col)

    /* Minimize Button (_) */
    DRAW_UBUNTU_BTN(min_x, btn_y, rgb565(60, 60, 66));
    fb_drawline(min_x + 3, btn_y + 9, min_x + 10, btn_y + 9, COLOR_WHITE);
    fb_drawline(min_x + 3, btn_y + 10, min_x + 10, btn_y + 10, COLOR_WHITE);

    /* Maximize Button (□) */
    DRAW_UBUNTU_BTN(max_x, btn_y, rgb565(60, 60, 66));
    if (win->is_maximized) {
        /* Restore symbol */
        fb_drawline(max_x + 5, btn_y + 3, max_x + 11, btn_y + 3, COLOR_WHITE);
        fb_drawline(max_x + 11, btn_y + 3, max_x + 11, btn_y + 8, COLOR_WHITE);
        fb_drawline(max_x + 3, btn_y + 5, max_x + 9, btn_y + 5, COLOR_WHITE);
        fb_drawline(max_x + 3, btn_y + 11, max_x + 9, btn_y + 11, COLOR_WHITE);
        fb_drawline(max_x + 3, btn_y + 5, max_x + 3, btn_y + 11, COLOR_WHITE);
        fb_drawline(max_x + 9, btn_y + 5, max_x + 9, btn_y + 11, COLOR_WHITE);
    } else {
        /* Maximize box */
        fb_drawline(max_x + 3, btn_y + 3, max_x + 10, btn_y + 3, COLOR_WHITE);
        fb_drawline(max_x + 3, btn_y + 10, max_x + 10, btn_y + 10, COLOR_WHITE);
        fb_drawline(max_x + 3, btn_y + 3, max_x + 3, btn_y + 10, COLOR_WHITE);
        fb_drawline(max_x + 10, btn_y + 3, max_x + 10, btn_y + 10, COLOR_WHITE);
    }

    /* Close Button (✕, Ubuntu Orange) */
    DRAW_UBUNTU_BTN(close_x, btn_y, rgb565(233, 84, 32));
    fb_drawline(close_x + 3, btn_y + 3, close_x + 10, btn_y + 10, COLOR_WHITE);
    fb_drawline(close_x + 4, btn_y + 3, close_x + 11, btn_y + 10, COLOR_WHITE);
    fb_drawline(close_x + 10, btn_y + 3, close_x + 3, btn_y + 10, COLOR_WHITE);
    fb_drawline(close_x + 11, btn_y + 3, close_x + 4, btn_y + 10, COLOR_WHITE);

    #undef DRAW_UBUNTU_BTN

    /* Title text (Left-aligned Ubuntu style) */
    int max_title_chars = (tbw - 70) / 8;
    if (max_title_chars < 1) max_title_chars = 1;
    char title_buf[32];
    int tl = 0;
    for (; tl < max_title_chars && win->title[tl] && tl < 30; tl++) {
        title_buf[tl] = win->title[tl];
    }
    title_buf[tl] = '\0';
    draw_text(tbx + 10, tby + 2, title_buf, COLOR_WHITE);

    
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

static void draw_app_icon_gfx(int ix, int iy, int id) {
    if (id == 0) {
        /* Web Browser: Modern macOS Safari Blue Badge */
        fb_fillrect(ix + 10, iy + 4, 44, 44, rgb565(32, 130, 235));
        fb_drawline(ix + 10, iy + 4, ix + 53, iy + 4, rgb565(90, 175, 255));
        /* Globe rings */
        fb_drawline(ix + 18, iy + 26, ix + 46, iy + 26, COLOR_WHITE);
        fb_drawline(ix + 32, iy + 12, ix + 32, iy + 40, COLOR_WHITE);
        fb_drawline(ix + 22, iy + 16, ix + 42, iy + 36, rgb565(190, 225, 255));
        fb_fillrect(ix + 30, iy + 24, 4, 4, rgb565(240, 70, 70));
    } else if (id == 1) {
        /* Terminal: Ubuntu Yaru Dark Slate Console with Green Prompt */
        fb_fillrect(ix + 10, iy + 4, 44, 44, rgb565(40, 42, 50));
        fb_drawline(ix + 10, iy + 4, ix + 53, iy + 4, rgb565(75, 80, 95));
        /* > _ */
        fb_putpixel(ix + 18, iy + 20, rgb565(50, 235, 100));
        fb_putpixel(ix + 19, iy + 21, rgb565(50, 235, 100));
        fb_putpixel(ix + 18, iy + 22, rgb565(50, 235, 100));
        fb_fillrect(ix + 23, iy + 22, 6, 2, rgb565(50, 235, 100));
        fb_fillrect(ix + 18, iy + 28, 20, 2, rgb565(140, 145, 160));
        fb_fillrect(ix + 18, iy + 34, 12, 2, rgb565(140, 145, 160));
    } else if (id == 2) {
        /* File Manager: Clean Theme-Matched Tall Ubuntu Folder (36x40px) */
        uint16_t f_main = (bg_color_idx == 0) ? rgb565(38, 132, 226) :
                          (bg_color_idx == 1) ? rgb565(26, 150, 132) :
                          (bg_color_idx == 2) ? rgb565(88, 92, 104) :
                          (bg_color_idx == 3) ? rgb565(210, 68, 50) : rgb565(235, 130, 30);
        uint16_t f_tab  = (bg_color_idx == 0) ? rgb565(22, 92, 170) :
                          (bg_color_idx == 1) ? rgb565(16, 102, 90) :
                          (bg_color_idx == 2) ? rgb565(58, 60, 70) :
                          (bg_color_idx == 3) ? rgb565(145, 40, 30) : rgb565(175, 85, 15);
        uint16_t f_hi   = (bg_color_idx == 0) ? rgb565(120, 188, 255) :
                          (bg_color_idx == 1) ? rgb565(110, 220, 200) :
                          (bg_color_idx == 2) ? rgb565(160, 165, 180) :
                          (bg_color_idx == 3) ? rgb565(255, 140, 120) : rgb565(255, 195, 110);
        fb_fillrect(ix + 14, iy + 6, 16, 4, f_tab);
        fb_fillrect(ix + 14, iy + 6, 16, 1, f_hi);
        fb_fillrect(ix + 14, iy + 9, 36, 37, f_tab);
        fb_fillrect(ix + 18, iy + 11, 28, 5, rgb565(250, 252, 255));
        fb_fillrect(ix + 14, iy + 15, 36, 31, f_main);
        fb_fillrect(ix + 14, iy + 15, 36, 1, f_hi);
        fb_fillrect(ix + 24, iy + 27, 16, 2, f_hi);
    } else if (id == 3) {
        /* Text Editor: Clean Off-White Document Card with Ubuntu Orange Header */
        fb_fillrect(ix + 12, iy + 4, 40, 44, rgb565(248, 249, 252));
        fb_fillrect(ix + 12, iy + 4, 40, 8, rgb565(235, 95, 30));
        fb_drawline(ix + 12, iy + 4, ix + 51, iy + 4, rgb565(255, 140, 80));
        fb_drawline(ix + 18, iy + 20, ix + 42, iy + 20, rgb565(165, 175, 195));
        fb_drawline(ix + 18, iy + 26, ix + 42, iy + 26, rgb565(165, 175, 195));
        fb_drawline(ix + 18, iy + 32, ix + 34, iy + 32, rgb565(165, 175, 195));
        /* Pencil tip */
        fb_fillrect(ix + 36, iy + 34, 6, 8, rgb565(235, 95, 30));
    } else if (id == 4) {
        /* Calculator: Modern Charcoal Keypad with Orange Accent */
        fb_fillrect(ix + 10, iy + 4, 44, 44, rgb565(42, 44, 52));
        fb_fillrect(ix + 14, iy + 8, 36, 10, rgb565(230, 235, 242));
        fb_fillrect(ix + 34, iy + 10, 12, 2, rgb565(30, 35, 45));
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                fb_fillrect(ix + 14 + c * 8, iy + 22 + r * 7, 6, 5, rgb565(85, 90, 105));
            }
        }
        fb_fillrect(ix + 39, iy + 22, 7, 19, rgb565(235, 95, 30));
    } else if (id == 5) {
        /* System Info: Cobalt Silicon Chip */
        fb_fillrect(ix + 12, iy + 6, 40, 40, rgb565(28, 80, 170));
        fb_drawline(ix + 12, iy + 6, ix + 51, iy + 6, rgb565(80, 150, 255));
        fb_fillrect(ix + 22, iy + 16, 20, 20, rgb565(15, 48, 110));
        fb_drawline(ix + 26, iy + 26, ix + 37, iy + 26, COLOR_WHITE);
        fb_drawline(ix + 32, iy + 20, ix + 32, iy + 31, COLOR_WHITE);
    } else if (id == 6) {
        /* Task Manager: Clean Pulse Waveform */
        fb_fillrect(ix + 10, iy + 4, 44, 44, rgb565(34, 36, 44));
        fb_drawline(ix + 10, iy + 4, ix + 53, iy + 4, rgb565(70, 75, 90));
        fb_drawline(ix + 14, iy + 26, ix + 22, iy + 26, rgb565(50, 235, 100));
        fb_drawline(ix + 22, iy + 26, ix + 26, iy + 14, rgb565(50, 235, 100));
        fb_drawline(ix + 26, iy + 14, ix + 30, iy + 38, rgb565(50, 235, 100));
        fb_drawline(ix + 30, iy + 38, ix + 34, iy + 22, rgb565(50, 235, 100));
        fb_drawline(ix + 34, iy + 22, ix + 38, iy + 26, rgb565(50, 235, 100));
        fb_drawline(ix + 38, iy + 26, ix + 50, iy + 26, rgb565(50, 235, 100));
    } else if (id == 7) {
        /* DOOM: Aubergine Gamepad Badge */
        fb_fillrect(ix + 10, iy + 4, 44, 44, rgb565(140, 25, 35));
        fb_drawline(ix + 10, iy + 4, ix + 53, iy + 4, rgb565(200, 60, 70));
        fb_fillrect(ix + 18, iy + 24, 12, 4, COLOR_WHITE);
        fb_fillrect(ix + 22, iy + 20, 4, 12, COLOR_WHITE);
        fb_fillrect(ix + 38, iy + 22, 6, 6, rgb565(235, 95, 30));
    } else if (id == 8) {
        /* Settings: Minimalist Precision Dual Gears */
        fb_fillrect(ix + 10, iy + 4, 44, 44, rgb565(60, 65, 75));
        fb_drawline(ix + 10, iy + 4, ix + 53, iy + 4, rgb565(100, 105, 120));
        fb_fillrect(ix + 28, iy + 14, 8, 24, rgb565(210, 215, 225));
        fb_fillrect(ix + 20, iy + 22, 24, 8, rgb565(210, 215, 225));
        fb_fillrect(ix + 24, iy + 18, 16, 16, rgb565(170, 175, 185));
        fb_fillrect(ix + 28, iy + 22, 8, 8, rgb565(40, 44, 52));
    }
}

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
    extern volatile int doom_running;
    extern volatile int doom_loading;
    if (!desk_loaded && !doom_running && !doom_loading) desk_load_files();

    /* Theme-matched folder palette (macOS / Ubuntu Yaru Style) */
    uint16_t f_main = (bg_color_idx == 0) ? rgb565(38, 132, 226) :
                      (bg_color_idx == 1) ? rgb565(26, 150, 132) :
                      (bg_color_idx == 2) ? rgb565(88, 92, 104) :
                      (bg_color_idx == 3) ? rgb565(210, 68, 50) : rgb565(235, 130, 30);
    uint16_t f_tab  = (bg_color_idx == 0) ? rgb565(22, 92, 170) :
                      (bg_color_idx == 1) ? rgb565(16, 102, 90) :
                      (bg_color_idx == 2) ? rgb565(58, 60, 70) :
                      (bg_color_idx == 3) ? rgb565(145, 40, 30) : rgb565(175, 85, 15);
    uint16_t f_hi   = (bg_color_idx == 0) ? rgb565(120, 188, 255) :
                      (bg_color_idx == 1) ? rgb565(110, 220, 200) :
                      (bg_color_idx == 2) ? rgb565(160, 165, 180) :
                      (bg_color_idx == 3) ? rgb565(255, 140, 120) : rgb565(255, 195, 110);
    uint16_t f_shadow = (bg_color_idx == 0) ? rgb565(15, 60, 125) :
                        (bg_color_idx == 1) ? rgb565(10, 70, 60) :
                        (bg_color_idx == 2) ? rgb565(35, 38, 45) :
                        (bg_color_idx == 3) ? rgb565(110, 25, 20) : rgb565(125, 60, 10);

    for (int i = 0; i < desk_count; i++) {
        if (!desk_files[i].valid) continue;
        int ix = desk_files[i].x;
        int iy = desk_files[i].y;
        if (iy + DESK_ICON_H > (int)fb_height) continue;

        if (desk_files[i].is_dir) {
            /* Clean Ubuntu Tall Folder (Good Height, Low Width: 36x40px) */
            /* Top-left tab */
            fb_fillrect(ix + 14, iy + 8, 16, 4, f_tab);
            fb_fillrect(ix + 14, iy + 8, 16, 1, f_hi);
            /* Back flap */
            fb_fillrect(ix + 14, iy + 11, 36, 37, f_tab);
            /* Clean white interior paper peek */
            fb_fillrect(ix + 18, iy + 13, 28, 5, rgb565(250, 252, 255));
            /* Front pocket */
            fb_fillrect(ix + 14, iy + 17, 36, 31, f_main);
            fb_fillrect(ix + 14, iy + 17, 36, 1, f_hi);
            /* Front accent groove */
            fb_fillrect(ix + 24, iy + 29, 16, 2, f_hi);
            /* Drop shadow */
            fb_drawline(ix + 14, iy + 48, ix + 49, iy + 48, f_shadow);
            fb_drawline(ix + 49, iy + 17, ix + 49, iy + 48, f_shadow);
        } else {
            int nlen=0; while(desk_files[i].name[nlen]) nlen++;
            int is_b = nlen>4 &&
                (desk_files[i].name[nlen-3]=='B'||desk_files[i].name[nlen-3]=='b'||
                 desk_files[i].name[nlen-3]=='W'||desk_files[i].name[nlen-3]=='w'||
                 desk_files[i].name[nlen-3]=='A'||desk_files[i].name[nlen-3]=='a');
            
            if (is_b) {
                /* Modern Minimalist Dark Slate Executable Card */
                fb_fillrect(ix + 15, iy + 10, 34, 38, rgb565(42, 44, 52));
                fb_drawline(ix + 15, iy + 10, ix + 48, iy + 10, rgb565(75, 80, 95));
                fb_drawline(ix + 15, iy + 10, ix + 15, iy + 47, rgb565(75, 80, 95));
                fb_drawline(ix + 15, iy + 47, ix + 48, iy + 47, rgb565(25, 27, 32));
                fb_drawline(ix + 48, iy + 10, ix + 48, iy + 47, rgb565(25, 27, 32));
                /* > _ prompt in Ubuntu orange */
                fb_putpixel(ix + 24, iy + 26, rgb565(235, 95, 30));
                fb_putpixel(ix + 25, iy + 27, rgb565(235, 95, 30));
                fb_putpixel(ix + 24, iy + 28, rgb565(235, 95, 30));
                fb_fillrect(ix + 29, iy + 28, 6, 2, rgb565(235, 95, 30));
            } else {
                /* Modern Minimalist Apple / Ubuntu Clean Off-White Document */
                fb_fillrect(ix + 15, iy + 10, 34, 38, rgb565(248, 249, 252));
                fb_drawline(ix + 15, iy + 10, ix + 48, iy + 10, rgb565(190, 195, 205));
                fb_drawline(ix + 15, iy + 10, ix + 15, iy + 47, rgb565(190, 195, 205));
                fb_drawline(ix + 15, iy + 47, ix + 48, iy + 47, rgb565(190, 195, 205));
                fb_drawline(ix + 48, iy + 10, ix + 48, iy + 47, rgb565(190, 195, 205));
                /* Dog-ear corner fold */
                fb_fillrect(ix + 39, iy + 10, 9, 9, rgb565(218, 222, 232));
                /* Subtle text lines */
                for (int l = 0; l < 3; l++) {
                    fb_fillrect(ix + 20, iy + 24 + l * 6, (l == 2) ? 14 : 22, 2, rgb565(165, 175, 190));
                }
            }
        }
        char lbl[12]; int j;
        for (j=0; j<8 && desk_files[i].name[j]; j++) lbl[j]=desk_files[i].name[j];
        lbl[j]='\0';
        int nlen = j;
        int pill_w = nlen * 8 + 8;
        int pill_x = ix + (ICON_W - pill_w) / 2;
        int pill_y = iy + 52;
        fb_fillrect(pill_x, pill_y, pill_w, 16, rgb565(22, 24, 30));
        fb_drawline(pill_x, pill_y, pill_x + pill_w - 1, pill_y, rgb565(65, 75, 95));
        draw_text(pill_x + 4, pill_y, lbl, COLOR_WHITE);
    }
    
    /* ---- 2b. Desktop Background Widgets Overlay (Pinned via [ATD]) ---- */
    extern void widgets_draw_desktop_overlay(void);
    widgets_draw_desktop_overlay();

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
    
    /* 3. Top Navigation Bar (Mac/Ubuntu Hybrid Style) */
    int ty = 0;
    fb_fillrect(0, ty, fb_width, TASKBAR_HEIGHT, rgb565(230, 230, 230)); /* Light gray bar */
    fb_drawline(0, ty + TASKBAR_HEIGHT - 1, fb_width, ty + TASKBAR_HEIGHT - 1, rgb565(150, 150, 150));
    
    /* 1. STAX Logo Button */
    int stax_btn_x = 6;
    int stax_btn_w = 32;
    uint16_t stax_bg = stax_menu_active ? rgb565(35, 110, 225) : rgb565(215, 218, 228);
    fb_fillrect(stax_btn_x, ty + 3, stax_btn_w, 22, stax_bg);
    fb_drawline(stax_btn_x, ty + 3, stax_btn_x + stax_btn_w - 1, ty + 3, stax_menu_active ? rgb565(70, 150, 255) : rgb565(190, 195, 205));
    fb_drawline(stax_btn_x, ty + 24, stax_btn_x + stax_btn_w - 1, ty + 24, stax_menu_active ? rgb565(20, 80, 180) : rgb565(190, 195, 205));
    fb_drawline(stax_btn_x, ty + 3, stax_btn_x, ty + 24, stax_menu_active ? rgb565(70, 150, 255) : rgb565(190, 195, 205));
    fb_drawline(stax_btn_x + stax_btn_w - 1, ty + 3, stax_btn_x + stax_btn_w - 1, ty + 24, stax_menu_active ? rgb565(20, 80, 180) : rgb565(190, 195, 205));

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
        draw_text(stax_btn_x + 8, ty + 6, "S", stax_menu_active ? COLOR_WHITE : COLOR_BLACK);
    }
    
    /* Vertical separator between STAX Menu and Apps Menu */
    fb_drawline(44, ty + 5, 44, ty + 22, rgb565(180, 185, 195));
    fb_drawline(45, ty + 5, 45, ty + 22, rgb565(245, 245, 250));

    /* 2. Apps Launcher Button */
    int app_btn_x = 50;
    int app_btn_w = 70;
    uint16_t app_bg = apps_menu_active ? rgb565(35, 110, 225) : rgb565(215, 218, 228);
    uint16_t app_fg = apps_menu_active ? COLOR_WHITE : rgb565(30, 35, 45);
    fb_fillrect(app_btn_x, ty + 3, app_btn_w, 22, app_bg);
    fb_drawline(app_btn_x, ty + 3, app_btn_x + app_btn_w - 1, ty + 3, apps_menu_active ? rgb565(70, 150, 255) : rgb565(190, 195, 205));
    fb_drawline(app_btn_x, ty + 24, app_btn_x + app_btn_w - 1, ty + 24, apps_menu_active ? rgb565(20, 80, 180) : rgb565(190, 195, 205));
    fb_drawline(app_btn_x, ty + 3, app_btn_x, ty + 24, apps_menu_active ? rgb565(70, 150, 255) : rgb565(190, 195, 205));
    fb_drawline(app_btn_x + app_btn_w - 1, ty + 3, app_btn_x + app_btn_w - 1, ty + 24, apps_menu_active ? rgb565(20, 80, 180) : rgb565(190, 195, 205));

    /* 9-dot grid icon (3x3 dots) */
    int mx0 = app_btn_x + 7, my0 = ty + 7;
    for (int dr = 0; dr < 3; dr++) {
        for (int dc = 0; dc < 3; dc++) {
            fb_fillrect(mx0 + dc * 4, my0 + dr * 4, 2, 2, app_fg);
        }
    }
    draw_text(app_btn_x + 24, ty + 6, "Apps", app_fg);

    /* Collect only open (non-hidden) windows for navigation tabs */
    window_t *tab_arr[32];
    int tab_count = 0;
    for (int i = 0; i < count; i++) {
        if (arr[i]->state != WM_STATE_HIDDEN) {
            tab_arr[tab_count++] = arr[i];
        }
    }

    /* Open Window Tabs in Navigation Bar */
    int nav_x = 128;
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

            uint16_t bg = is_active ? rgb565(35, 110, 225) : (is_min ? rgb565(215, 218, 225) : rgb565(238, 240, 246));
            uint16_t fg = is_active ? COLOR_WHITE : (is_min ? rgb565(130, 135, 145) : rgb565(35, 40, 55));
            uint16_t border = is_active ? rgb565(20, 80, 180) : rgb565(200, 205, 215);

            fb_fillrect(tx, ty + 3, tab_w, 22, bg);
            fb_drawline(tx, ty + 3, tx + tab_w - 1, ty + 3, border);
            fb_drawline(tx, ty + 3, tx, ty + 24, border);
            fb_drawline(tx + tab_w - 1, ty + 3, tx + tab_w - 1, ty + 24, border);
            fb_drawline(tx, ty + 24, tx + tab_w - 1, ty + 24, border);

            /* Status dot */
            uint16_t dot_col = is_active ? rgb565(80, 240, 100) : (is_min ? rgb565(170, 175, 185) : rgb565(80, 150, 240));
            fb_fillrect(tx + 5, ty + 12, 4, 4, dot_col);

            /* Truncate title cleanly to prevent any text overflow */
            int max_chars = (tab_w - 16) / 8;
            if (max_chars < 1) max_chars = 1;
            char title_trunc[18];
            int tlen = (int)strlen(w->title);
            if (tlen > max_chars && max_chars > 2) {
                int k = 0;
                for (; k < max_chars - 2 && k < 15; k++) {
                    title_trunc[k] = w->title[k];
                }
                title_trunc[k++] = '.';
                title_trunc[k++] = '.';
                title_trunc[k] = '\0';
            } else {
                int k = 0;
                for (; k < max_chars && w->title[k]; k++) title_trunc[k] = w->title[k];
                title_trunc[k] = '\0';
            }

            draw_text(tx + 12, ty + 6, title_trunc, fg);
        }

        /* Overflow pill if more tabs exist */
        if (overflow_count > 0) {
            int ox = nav_x + visible_tabs * (tab_w + tab_gap);
            fb_fillrect(ox, ty + 3, 38, 22, rgb565(220, 224, 232));
            fb_drawline(ox, ty + 3, ox + 37, ty + 3, rgb565(180, 185, 195));
            fb_drawline(ox, ty + 24, ox + 37, ty + 24, rgb565(180, 185, 195));
            fb_drawline(ox, ty + 3, ox, ty + 24, rgb565(180, 185, 195));
            fb_drawline(ox + 37, ty + 3, ox + 37, ty + 24, rgb565(180, 185, 195));

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
            draw_text(ox + 6, ty + 6, ovf_str, rgb565(40, 50, 70));
        }
    }
    
    /* Real-Time Date & Time (IST Mumbai) */
    char dt_str[32];
    extern void rtc_format_ist_navbar(char *buf, int max_len);
    rtc_format_ist_navbar(dt_str, sizeof(dt_str));
    draw_text(fb_width - 192, ty + 6, dt_str, COLOR_BLACK);
    
    /* Memory Usage */
    extern int get_total_memory(void);
    extern int get_free_memory(void);
    uint32_t tot = get_total_memory();
    uint32_t f = get_free_memory();
    int pct = 0;
    if (tot > 0) pct = ((tot - f) * 100) / tot;
    char mem_str[12];
    mem_str[0] = 'M'; mem_str[1] = 'E'; mem_str[2] = 'M'; mem_str[3] = ':'; mem_str[4] = ' ';
    int m_idx = 5;
    if (pct == 100) {
        mem_str[m_idx++] = '1'; mem_str[m_idx++] = '0'; mem_str[m_idx++] = '0';
    } else {
        if (pct >= 10) mem_str[m_idx++] = '0' + (pct / 10);
        mem_str[m_idx++] = '0' + (pct % 10);
    }
    mem_str[m_idx++] = '%';
    mem_str[m_idx] = '\0';
    draw_text(fb_width - 272, ty + 6, mem_str, COLOR_BLACK);
    
    /* Context Menu */
    if (ctx_menu.active) {
        fb_fillrect(ctx_menu.x, ctx_menu.y, 160, 240, rgb565(235, 235, 240));
        fb_drawline(ctx_menu.x, ctx_menu.y, ctx_menu.x + 159, ctx_menu.y, COLOR_WHITE);
        fb_drawline(ctx_menu.x, ctx_menu.y, ctx_menu.x, ctx_menu.y + 239, COLOR_WHITE);
        fb_drawline(ctx_menu.x + 159, ctx_menu.y, ctx_menu.x + 159, ctx_menu.y + 239, rgb565(120, 120, 130));
        fb_drawline(ctx_menu.x, ctx_menu.y + 239, ctx_menu.x + 159, ctx_menu.y + 239, rgb565(120, 120, 130));
        
        draw_text(ctx_menu.x + 10, ctx_menu.y + 8, "New Terminal", COLOR_BLACK);
        fb_drawline(ctx_menu.x + 5, ctx_menu.y + 30, ctx_menu.x + 155, ctx_menu.y + 30, rgb565(180, 180, 190));
        draw_text(ctx_menu.x + 10, ctx_menu.y + 38, "Retro Widgets", rgb565(20, 100, 220));
        fb_drawline(ctx_menu.x + 5, ctx_menu.y + 60, ctx_menu.x + 155, ctx_menu.y + 60, rgb565(180, 180, 190));
        draw_text(ctx_menu.x + 10, ctx_menu.y + 68, "Web Browser", COLOR_BLACK);
        fb_drawline(ctx_menu.x + 5, ctx_menu.y + 90, ctx_menu.x + 155, ctx_menu.y + 90, rgb565(180, 180, 190));
        draw_text(ctx_menu.x + 10, ctx_menu.y + 98, "File Manager", COLOR_BLACK);
        fb_drawline(ctx_menu.x + 5, ctx_menu.y + 120, ctx_menu.x + 155, ctx_menu.y + 120, rgb565(180, 180, 190));
        draw_text(ctx_menu.x + 10, ctx_menu.y + 128, "Text Editor", COLOR_BLACK);
        fb_drawline(ctx_menu.x + 5, ctx_menu.y + 150, ctx_menu.x + 155, ctx_menu.y + 150, rgb565(180, 180, 190));
        draw_text(ctx_menu.x + 10, ctx_menu.y + 158, "Calculator", COLOR_BLACK);
        fb_drawline(ctx_menu.x + 5, ctx_menu.y + 180, ctx_menu.x + 155, ctx_menu.y + 180, rgb565(180, 180, 190));
        draw_text(ctx_menu.x + 10, ctx_menu.y + 188, "Settings", COLOR_BLACK);
        fb_drawline(ctx_menu.x + 5, ctx_menu.y + 210, ctx_menu.x + 155, ctx_menu.y + 210, rgb565(180, 180, 190));
        draw_text(ctx_menu.x + 10, ctx_menu.y + 218, "Reboot System", rgb565(190, 40, 40));
    }

    /* 1. STAX System Dropdown Menu (Under Logo) */
    if (stax_menu_active) {
        int sm_x = 0;
        int sm_y = TASKBAR_HEIGHT;
        int sm_w = 190;
        int sm_h = 225;

        /* Drop shadow */
        fb_fillrect(sm_x + 3, sm_y + 3, sm_w, sm_h, rgb565(20, 22, 28));

        /* Menu card */
        fb_fillrect(sm_x, sm_y, sm_w, sm_h, rgb565(36, 38, 46));
        fb_drawline(sm_x, sm_y, sm_x + sm_w - 1, sm_y, rgb565(70, 75, 90));
        fb_drawline(sm_x, sm_y, sm_x, sm_y + sm_h - 1, rgb565(70, 75, 90));
        fb_drawline(sm_x + sm_w - 1, sm_y, sm_x + sm_w - 1, sm_y + sm_h - 1, rgb565(20, 22, 28));
        fb_drawline(sm_x, sm_y + sm_h - 1, sm_x + sm_w - 1, sm_y + sm_h - 1, rgb565(20, 22, 28));

        /* Menu Items */
        draw_text(sm_x + 12, sm_y + 8, "About STAX OS", COLOR_WHITE);
        fb_drawline(sm_x + 6, sm_y + 30, sm_x + sm_w - 6, sm_y + 30, rgb565(55, 58, 70));

        draw_text(sm_x + 12, sm_y + 38, "Retro Widgets", rgb565(80, 210, 255));
        draw_text(sm_x + 12, sm_y + 68, "System Settings", COLOR_WHITE);
        draw_text(sm_x + 12, sm_y + 98, "Task Manager", COLOR_WHITE);
        draw_text(sm_x + 12, sm_y + 128, "System Info", COLOR_WHITE);

        fb_drawline(sm_x + 6, sm_y + 156, sm_x + sm_w - 6, sm_y + 156, rgb565(55, 58, 70));

        draw_text(sm_x + 12, sm_y + 166, "Reboot System", rgb565(240, 80, 80));
        draw_text(sm_x + 12, sm_y + 196, "Force Quit...", rgb565(160, 165, 180));
    }
    
    /* 2. Modern 3x3 Applications Launcher Menu Window (Under Apps button) */
    if (apps_menu_active) {
        int app_x = 50;
        int app_y = TASKBAR_HEIGHT + 2;
        int app_w = 340;
        int app_h = 390;
        
        /* Drop shadow */
        fb_fillrect(app_x + 4, app_y + 4, app_w, app_h, rgb565(20, 22, 28));

        /* Card background */
        fb_fillrect(app_x, app_y, app_w, app_h, rgb565(32, 34, 42));
        fb_drawline(app_x, app_y, app_x + app_w - 1, app_y, rgb565(65, 70, 85));
        fb_drawline(app_x, app_y, app_x, app_y + app_h - 1, rgb565(65, 70, 85));
        fb_drawline(app_x + app_w - 1, app_y, app_x + app_w - 1, app_y + app_h - 1, rgb565(18, 20, 26));
        fb_drawline(app_x, app_y + app_h - 1, app_x + app_w - 1, app_y + app_h - 1, rgb565(18, 20, 26));
        
        /* Header */
        draw_text(app_x + 14, app_y + 10, "Applications & Tools", COLOR_WHITE);
        draw_text(app_x + app_w - 110, app_y + 10, "STAX OS", rgb565(140, 150, 175));
        fb_drawline(app_x + 10, app_y + 30, app_x + app_w - 10, app_y + 30, rgb565(50, 54, 66));

        /* 3 Columns x 3 Rows App Grid */
        const char *app_names[9] = {
            "Browser", "Terminal", "Files",
            "Editor",  "Calc",     "Sys Info",
            "Tasks",   "DOOM",     "Settings"
        };

        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                int aid = r * 3 + c;
                int ix = app_x + 12 + c * 106;
                int iy = app_y + 38 + r * 98;
                int tile_w = 98;
                int tile_h = 90;

                /* Tile frame */
                fb_fillrect(ix, iy, tile_w, tile_h, rgb565(42, 45, 56));
                fb_drawline(ix, iy, ix + tile_w - 1, iy, rgb565(60, 65, 80));
                fb_drawline(ix, iy, ix, iy + tile_h - 1, rgb565(60, 65, 80));
                fb_drawline(ix + tile_w - 1, iy, ix + tile_w - 1, iy + tile_h - 1, rgb565(25, 28, 36));
                fb_drawline(ix, iy + tile_h - 1, ix + tile_w - 1, iy + tile_h - 1, rgb565(25, 28, 36));

                /* Graphical App Icon */
                draw_app_icon_gfx(ix + 17, iy + 6, aid);

                /* App Label */
                const char *lbl = app_names[aid];
                int llen = (int)strlen(lbl);
                int lx = ix + (tile_w - llen * 8) / 2;
                draw_text(lx, iy + 64, lbl, COLOR_WHITE);
            }
        }

        /* Bottom System Controls */
        fb_drawline(app_x + 10, app_y + 342, app_x + app_w - 10, app_y + 342, rgb565(50, 54, 66));
        
        fb_fillrect(app_x + 12, app_y + 352, 98, 26, rgb565(45, 48, 60));
        draw_text(app_x + 22, app_y + 357, "About STAX", rgb565(200, 205, 220));

        fb_fillrect(app_x + 118, app_y + 352, 104, 26, rgb565(190, 45, 45));
        draw_text(app_x + 126, app_y + 357, "Reboot OS", COLOR_WHITE);

        fb_fillrect(app_x + 230, app_y + 352, 98, 26, rgb565(45, 48, 60));
        draw_text(app_x + 242, app_y + 357, "Force Quit", rgb565(200, 205, 220));
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
    
    /* 5. Swap */
    fb_swap();
}

