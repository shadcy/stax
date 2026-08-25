/* ============================================================================
 * STAX — wm_render.c
 * Window Manager Rendering
 * ============================================================================ */

#include "wm_internal.h"

int bg_color_idx = 0;
uint16_t bg_colors[5] = {
    RGB565_C(58, 110, 165),  /* Classic Blue (Windows NT/98 default) */
    RGB565_C(0, 128, 128),   /* Default Teal (Windows 95 Classic) */
    RGB565_C(0, 0, 0),       /* Black */
    RGB565_C(128, 0, 0),     /* Dark Red */
    RGB565_C(128, 128, 128)  /* Gray */
};

app_icon_t app_icons[NUM_APPS] = {
    {0, 18, 42,  "Browser"},
    {1, 18, 128, "Terminal"},
    {2, 18, 214, "Files"},
    {3, 18, 300, "Notes"},
    {4, 18, 386, "Calculator"},
    {5, 104, 42,  "Sys Info"},
    {6, 104, 128, "Task Mgr"},
    {7, 104, 214, "DOOM"}
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
    
    /* Titlebar */
    int tbx = wx + BORDER_WIDTH;
    int tby = wy + BORDER_WIDTH;
    int tbw = ww - BORDER_WIDTH*2;
    fb_fillrect(tbx, tby, tbw, TITLEBAR_HEIGHT, COL_WIN_TITLE);
    
    /* Title text (Centered) */
    int text_w = 0;
    while(win->title[text_w]) text_w++;
    int text_x = tbx + (tbw - (text_w * 8)) / 2;
    if (text_x < tbx + 60) text_x = tbx + 60;
    draw_text(text_x, tby + 2, win->title, COL_WIN_TITLE_TXT);
    
    /* MacOS Style Buttons (Close, Minimize, Maximize) */
    int btn_w = 12;
    int close_x = wx + BORDER_WIDTH + 8;
    int min_x   = close_x + btn_w + 6;
    int max_x   = min_x + btn_w + 6;
    
    /* Helper to draw rounded button */
    #define DRAW_MAC_BTN(bx, col) \
        fb_fillrect((bx)+2, tby+4, 8, 12, col); \
        fb_fillrect((bx), tby+6, 12, 8, col); \
        fb_fillrect((bx)+1, tby+5, 10, 10, col)
        
    /* Close Button (Red) */
    DRAW_MAC_BTN(close_x, rgb565(255, 95, 86));
    /* Minimize Button (Yellow) */
    DRAW_MAC_BTN(min_x, rgb565(255, 189, 46));
    /* Maximize Button (Green) */
    DRAW_MAC_BTN(max_x, rgb565(39, 201, 63));
    
    #undef DRAW_MAC_BTN
    
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
        /* Web Browser (Globe with orbital lines & compass needle) */
        fb_fillrect(ix+8, iy+4, 48, 44, rgb565(25, 90, 200));
        fb_fillrect(ix+12, iy+8, 40, 36, rgb565(35, 130, 240));
        fb_fillrect(ix+16, iy+10, 32, 4, rgb565(130, 200, 255));
        fb_drawline(ix+12, iy+26, ix+51, iy+26, rgb565(190, 230, 255));
        fb_drawline(ix+32, iy+8, ix+32, iy+43, rgb565(190, 230, 255));
        fb_drawline(ix+18, iy+14, ix+46, iy+38, rgb565(230, 245, 255));
        fb_fillrect(ix+30, iy+22, 5, 8, rgb565(255, 60, 60));
        fb_fillrect(ix+31, iy+20, 3, 3, COLOR_WHITE);
    } else if (id == 1) {
        /* Terminal (Dark monitor with glowing green prompt) */
        fb_fillrect(ix+8, iy+4, 48, 44, rgb565(45, 48, 56));
        fb_fillrect(ix+10, iy+6, 44, 2, rgb565(90, 95, 110));
        fb_fillrect(ix+12, iy+10, 40, 32, rgb565(12, 14, 20));
        fb_putpixel(ix+16, iy+16, rgb565(40, 240, 80));
        fb_putpixel(ix+17, iy+17, rgb565(40, 240, 80));
        fb_putpixel(ix+16, iy+18, rgb565(40, 240, 80));
        fb_fillrect(ix+20, iy+18, 6, 2, rgb565(40, 240, 80));
        fb_fillrect(ix+16, iy+24, 22, 2, rgb565(180, 190, 200));
        fb_fillrect(ix+16, iy+30, 14, 2, rgb565(180, 190, 200));
    } else if (id == 2) {
        /* File Manager (Golden Amber Folder) */
        fb_fillrect(ix+10, iy+8, 22, 10, rgb565(220, 140, 0));
        fb_fillrect(ix+16, iy+6, 32, 20, rgb565(250, 250, 255));
        fb_drawline(ix+20, iy+10, ix+38, iy+10, rgb565(180, 180, 210));
        fb_fillrect(ix+8, iy+16, 48, 32, rgb565(255, 185, 25));
        fb_fillrect(ix+8, iy+16, 48, 3, rgb565(255, 220, 90));
        fb_drawline(ix+8, iy+47, ix+55, iy+47, rgb565(180, 110, 0));
    } else if (id == 3) {
        /* Notes / Text Editor (Notepad with Pencil) */
        fb_fillrect(ix+12, iy+4, 38, 44, rgb565(255, 252, 235));
        fb_fillrect(ix+12, iy+4, 38, 8, rgb565(235, 75, 40));
        fb_drawline(ix+20, iy+12, ix+20, iy+47, rgb565(240, 100, 100));
        fb_drawline(ix+23, iy+18, ix+46, iy+18, rgb565(180, 200, 230));
        fb_drawline(ix+23, iy+24, ix+46, iy+24, rgb565(180, 200, 230));
        fb_drawline(ix+23, iy+30, ix+46, iy+30, rgb565(180, 200, 230));
        fb_fillrect(ix+36, iy+26, 10, 18, rgb565(245, 185, 20));
        fb_fillrect(ix+36, iy+24, 10, 3, rgb565(255, 140, 160));
        fb_fillrect(ix+39, iy+44, 4, 3, rgb565(50, 50, 50));
    } else if (id == 4) {
        /* Calculator (Modern Keypad & LCD Display) */
        fb_fillrect(ix+10, iy+4, 44, 44, rgb565(50, 54, 65));
        fb_drawline(ix+10, iy+4, ix+53, iy+4, rgb565(90, 95, 110));
        fb_fillrect(ix+14, iy+8, 36, 10, rgb565(220, 235, 225));
        fb_fillrect(ix+36, iy+10, 10, 2, rgb565(40, 60, 50));
        fb_fillrect(ix+14, iy+22, 6, 6, rgb565(100, 105, 120));
        fb_fillrect(ix+23, iy+22, 6, 6, rgb565(100, 105, 120));
        fb_fillrect(ix+32, iy+22, 6, 6, rgb565(100, 105, 120));
        fb_fillrect(ix+14, iy+31, 6, 6, rgb565(100, 105, 120));
        fb_fillrect(ix+23, iy+31, 6, 6, rgb565(100, 105, 120));
        fb_fillrect(ix+32, iy+31, 6, 6, rgb565(100, 105, 120));
        fb_fillrect(ix+41, iy+22, 9, 20, rgb565(245, 130, 30));
    } else if (id == 5) {
        /* System Info (Processor Chip with Golden Pins) */
        fb_fillrect(ix+12, iy+8, 40, 36, rgb565(20, 60, 140));
        fb_fillrect(ix+14, iy+10, 36, 3, rgb565(90, 150, 240));
        fb_fillrect(ix+16, iy+4, 3, 4, rgb565(240, 190, 30));
        fb_fillrect(ix+24, iy+4, 3, 4, rgb565(240, 190, 30));
        fb_fillrect(ix+32, iy+4, 3, 4, rgb565(240, 190, 30));
        fb_fillrect(ix+40, iy+4, 3, 4, rgb565(240, 190, 30));
        fb_fillrect(ix+16, iy+44, 3, 4, rgb565(240, 190, 30));
        fb_fillrect(ix+24, iy+44, 3, 4, rgb565(240, 190, 30));
        fb_fillrect(ix+32, iy+44, 3, 4, rgb565(240, 190, 30));
        fb_fillrect(ix+40, iy+44, 3, 4, rgb565(240, 190, 30));
        fb_fillrect(ix+20, iy+18, 24, 16, rgb565(10, 35, 90));
        fb_drawline(ix+24, iy+26, ix+39, iy+26, rgb565(120, 180, 255));
    } else if (id == 6) {
        /* Task Manager (ECG Heartbeat Pulse Monitor) */
        fb_fillrect(ix+8, iy+4, 48, 44, rgb565(30, 32, 40));
        fb_fillrect(ix+12, iy+8, 40, 36, rgb565(10, 16, 22));
        fb_drawline(ix+12, iy+26, ix+51, iy+26, rgb565(25, 40, 50));
        fb_drawline(ix+14, iy+26, ix+22, iy+26, rgb565(40, 245, 100));
        fb_drawline(ix+22, iy+26, ix+26, iy+14, rgb565(40, 245, 100));
        fb_drawline(ix+26, iy+14, ix+30, iy+38, rgb565(40, 245, 100));
        fb_drawline(ix+30, iy+38, ix+34, iy+20, rgb565(40, 245, 100));
        fb_drawline(ix+34, iy+20, ix+38, iy+26, rgb565(40, 245, 100));
        fb_drawline(ix+38, iy+26, ix+50, iy+26, rgb565(40, 245, 100));
    } else if (id == 7) {
        /* DOOM / Games (Arcade Controller Badge) */
        fb_fillrect(ix+8, iy+4, 48, 44, rgb565(170, 25, 25));
        fb_fillrect(ix+12, iy+8, 40, 36, rgb565(30, 25, 30));
        fb_fillrect(ix+16, iy+24, 12, 4, rgb565(220, 220, 230));
        fb_fillrect(ix+20, iy+20, 4, 12, rgb565(220, 220, 230));
        fb_fillrect(ix+38, iy+18, 5, 5, rgb565(40, 160, 255));
        fb_fillrect(ix+34, iy+25, 5, 5, rgb565(255, 200, 30));
        fb_fillrect(ix+42, iy+25, 5, 5, rgb565(255, 50, 50));
    }
}

void wm_render(void) {
    /* ---- 1. Desktop background ---- */
    if (desktop_bg_image) {
        extern uint16_t *fb_get_buffer(void);
        uint16_t *fbuf = fb_get_buffer();
        if (fbuf) {
            memcpy(fbuf, desktop_bg_image, fb_width * fb_height * 2);
        }
    } else {
        fb_clear(COL_DESKTOP);
    }

    /* ---- 2a. App shortcut icons ---- */
    for (int i = 0; i < NUM_APPS; i++) {
        int ix = app_icons[i].x;
        int iy = app_icons[i].y;

        draw_app_icon_gfx(ix, iy, app_icons[i].id);

        int nlen = (int)strlen(app_icons[i].name);
        int pill_w = nlen * 8 + 8;
        int pill_x = ix + (ICON_W - pill_w) / 2;
        int pill_y = iy + 52;
        fb_fillrect(pill_x, pill_y, pill_w, 16, rgb565(20, 25, 35));
        fb_drawline(pill_x, pill_y, pill_x + pill_w - 1, pill_y, rgb565(60, 70, 90));
        draw_text(pill_x + 4, pill_y, app_icons[i].name, COLOR_WHITE);
    }
    
    /* ---- 2b. Filesystem icons ---- */
    extern volatile int doom_running;
    extern volatile int doom_loading;
    if (!desk_loaded && !doom_running && !doom_loading) desk_load_files();
    for (int i = 0; i < desk_count; i++) {
        if (!desk_files[i].valid) continue;
        int ix = desk_files[i].x;
        int iy = desk_files[i].y;
        if (iy + DESK_ICON_H > (int)fb_height) continue;

        if (desk_files[i].is_dir) {
            static uint16_t *folder_icon = NULL;
            static int folder_w = 0, folder_h = 0;
            static int load_attempted = 0;
            if (!folder_icon && !load_attempted) {
                extern uint16_t *bmp_load(const char *filename, int *out_w, int *out_h);
                folder_icon = bmp_load("BMP/FOLDER.BMP", &folder_w, &folder_h);
                load_attempted = 1;
            }
            if (folder_icon) {
                for (int fy = 0; fy < folder_h; fy++) {
                    for (int fx = 0; fx < folder_w; fx++) {
                        uint16_t p = folder_icon[fy * folder_w + fx];
                        fb_putpixel(ix + (64 - folder_w)/2 + fx, iy + 6 + fy, p);
                    }
                }
            } else {
                fb_fillrect(ix+8, iy+12, 24, 8, rgb565(230,160,0));
                fb_fillrect(ix+14, iy+8, 28, 16, rgb565(245,245,255));
                fb_drawline(ix+18, iy+12, ix+32, iy+12, rgb565(200,200,220));
                fb_drawline(ix+18, iy+16, ix+38, iy+16, rgb565(200,200,220));
                fb_fillrect(ix+6, iy+20, 52, 28, rgb565(255,200,40));
                fb_fillrect(ix+6, iy+20, 52, 3, rgb565(255,225,100));
            }
        } else {
            int nlen=0; while(desk_files[i].name[nlen]) nlen++;
            int is_t = nlen>4 &&
                (desk_files[i].name[nlen-3]=='T'||desk_files[i].name[nlen-3]=='t') &&
                (desk_files[i].name[nlen-2]=='X'||desk_files[i].name[nlen-2]=='x') &&
                (desk_files[i].name[nlen-1]=='T'||desk_files[i].name[nlen-1]=='t');
            int is_b = nlen>4 &&
                (desk_files[i].name[nlen-3]=='B'||desk_files[i].name[nlen-3]=='b') &&
                (desk_files[i].name[nlen-2]=='I'||desk_files[i].name[nlen-2]=='i') &&
                (desk_files[i].name[nlen-1]=='N'||desk_files[i].name[nlen-1]=='n');
            
            uint16_t page_col = is_t ? rgb565(240,245,255) :
                                is_b ? rgb565(235,235,240) :
                                       rgb565(245,245,250);
            uint16_t border   = is_t ? rgb565(100,120,220) :
                                is_b ? rgb565(120,120,130) :
                                       rgb565(150,150,160);
                                       
            fb_fillrect(ix+14, iy+6, 32, 42, page_col);
            
            fb_drawline(ix+14, iy+6, ix+34, iy+6, border);
            fb_drawline(ix+14, iy+6, ix+14, iy+47, border);
            fb_drawline(ix+14, iy+47, ix+45, iy+47, border);
            fb_drawline(ix+45, iy+17, ix+45, iy+47, border);
            fb_drawline(ix+34, iy+6, ix+45, iy+17, border);
            
            fb_fillrect(ix+35, iy+7, 10, 10, rgb565(210,210,220));
            fb_drawline(ix+34, iy+17, ix+45, iy+17, border);
            fb_drawline(ix+34, iy+6, ix+34, iy+17, border);
            if (is_t) {
                for (int l=0;l<4;l++) fb_fillrect(ix+18, iy+20+l*6, 22, 2, border);
            } else if (is_b) {
                fb_fillrect(ix+18, iy+22, 22, 3, rgb565(180,180,190));
                fb_fillrect(ix+18, iy+28, 14, 3, rgb565(180,180,190));
                fb_fillrect(ix+18, iy+34, 18, 3, rgb565(180,180,190));
            }
        }
        char lbl[12]; int j;
        for (j=0; j<8 && desk_files[i].name[j]; j++) lbl[j]=desk_files[i].name[j];
        lbl[j]='\0';
        int nlen = j;
        int pill_w = nlen * 8 + 8;
        int pill_x = ix + (ICON_W - pill_w) / 2;
        int pill_y = iy + 52;
        fb_fillrect(pill_x, pill_y, pill_w, 16, rgb565(20, 25, 35));
        fb_drawline(pill_x, pill_y, pill_x + pill_w - 1, pill_y, rgb565(60, 70, 90));
        draw_text(pill_x + 4, pill_y, lbl, COLOR_WHITE);
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
    
    /* 3. Top Menu Bar (Mac-Style) */
    int ty = 0;
    fb_fillrect(0, ty, fb_width, TASKBAR_HEIGHT, rgb565(230, 230, 230)); /* Light gray bar */
    fb_drawline(0, ty + TASKBAR_HEIGHT - 1, fb_width, ty + TASKBAR_HEIGHT - 1, rgb565(150, 150, 150)); /* Bottom border */
    
    /* STAX Menu Logo (Instead of Start button) */
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
                fb_putpixel(12 + fx, ty + (TASKBAR_HEIGHT - logo_h) / 2 + fy, p);
            }
        }
    } else {
        fb_fillrect(8, ty + 6, 16, 16, COLOR_BLACK); /* STAX Logo placeholder */
        draw_text(32, ty + 6, "STAX", COLOR_BLACK);
    }
    
    /* Active Window Title / Menu items */
    if (count > 0 && arr[0]->state != WM_STATE_HIDDEN) {
        draw_text(90, ty + 6, arr[0]->title, COLOR_BLACK);
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
        fb_fillrect(ctx_menu.x, ctx_menu.y, 160, 180, rgb565(235, 235, 240));
        fb_drawline(ctx_menu.x, ctx_menu.y, ctx_menu.x + 159, ctx_menu.y, COLOR_WHITE);
        fb_drawline(ctx_menu.x, ctx_menu.y, ctx_menu.x, ctx_menu.y + 179, COLOR_WHITE);
        fb_drawline(ctx_menu.x + 159, ctx_menu.y, ctx_menu.x + 159, ctx_menu.y + 179, rgb565(120, 120, 130));
        fb_drawline(ctx_menu.x, ctx_menu.y + 179, ctx_menu.x + 159, ctx_menu.y + 179, rgb565(120, 120, 130));
        
        draw_text(ctx_menu.x + 10, ctx_menu.y + 8, "New Terminal", COLOR_BLACK);
        fb_drawline(ctx_menu.x + 5, ctx_menu.y + 30, ctx_menu.x + 155, ctx_menu.y + 30, rgb565(180, 180, 190));
        draw_text(ctx_menu.x + 10, ctx_menu.y + 38, "Web Browser", COLOR_BLACK);
        fb_drawline(ctx_menu.x + 5, ctx_menu.y + 60, ctx_menu.x + 155, ctx_menu.y + 60, rgb565(180, 180, 190));
        draw_text(ctx_menu.x + 10, ctx_menu.y + 68, "File Manager", COLOR_BLACK);
        fb_drawline(ctx_menu.x + 5, ctx_menu.y + 90, ctx_menu.x + 155, ctx_menu.y + 90, rgb565(180, 180, 190));
        draw_text(ctx_menu.x + 10, ctx_menu.y + 98, "Text Editor", COLOR_BLACK);
        fb_drawline(ctx_menu.x + 5, ctx_menu.y + 120, ctx_menu.x + 155, ctx_menu.y + 120, rgb565(180, 180, 190));
        draw_text(ctx_menu.x + 10, ctx_menu.y + 128, "Calculator", COLOR_BLACK);
        fb_drawline(ctx_menu.x + 5, ctx_menu.y + 150, ctx_menu.x + 155, ctx_menu.y + 150, rgb565(180, 180, 190));
        draw_text(ctx_menu.x + 10, ctx_menu.y + 158, "Change Wallpaper", COLOR_BLACK);
    }
    
    /* STAX System Menu Dropdown */
    if (start_menu_active) {
        int sm_x = 0;
        int sm_y = TASKBAR_HEIGHT; /* Dropdown from top bar */
        int sm_w = 200;
        int sm_h = 210;
        
        fb_fillrect(sm_x, sm_y, sm_w, sm_h, rgb565(245, 245, 250));
        fb_drawline(sm_x, sm_y, sm_x + sm_w, sm_y, rgb565(160, 160, 170));
        fb_drawline(sm_x + sm_w, sm_y, sm_x + sm_w, sm_y + sm_h, rgb565(160, 160, 170));
        fb_drawline(sm_x, sm_y + sm_h, sm_x + sm_w, sm_y + sm_h, rgb565(160, 160, 170));
        
        /* Options */
        draw_text(sm_x + 10, sm_y + 8, "About STAX OS", COLOR_BLACK);
        fb_drawline(sm_x + 5, sm_y + 30, sm_x + sm_w - 5, sm_y + 30, rgb565(200, 200, 210));
        
        draw_text(sm_x + 10, sm_y + 38, "Web Browser", COLOR_BLACK);
        draw_text(sm_x + 10, sm_y + 68, "Terminal", COLOR_BLACK);
        draw_text(sm_x + 10, sm_y + 98, "File Manager", COLOR_BLACK);
        draw_text(sm_x + 10, sm_y + 128, "Task Manager", COLOR_BLACK);
        
        fb_drawline(sm_x + 5, sm_y + 150, sm_x + sm_w - 5, sm_y + 150, rgb565(200, 200, 210));
        
        draw_text(sm_x + 10, sm_y + 158, "Change Wallpaper", COLOR_BLACK);
        draw_text(sm_x + 10, sm_y + 188, "Force Quit...", rgb565(180, 30, 30));
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
