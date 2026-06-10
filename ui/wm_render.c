/* ============================================================================
 * TIOS — wm_render.c
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
    {0, 10, 10, "Boot Log"},
    {1, 10, 90, "File Mgr"},
    {2, 10, 170, "sh** slime"}
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
    
    /* Title text */
    draw_text(tbx + 4, tby + 2, win->title, COL_WIN_TITLE_TXT);
    
    /* Buttons (Minimize, Maximize, Close) */
    int btn_w = 16;
    int close_x = wx + ww - BORDER_WIDTH - btn_w - 2;
    int max_x   = close_x - btn_w - 2;
    int min_x   = max_x - btn_w - 2;
    
    /* Close Button */
    fb_fillrect(close_x, tby + 2, btn_w, btn_w, COL_WIN_BG);
    draw_text(close_x + 4, tby + 2, "X", COLOR_BLACK);
    
    /* Maximize Button */
    fb_fillrect(max_x, tby + 2, btn_w, btn_w, COL_WIN_BG);
    if (win->is_maximized) {
        /* Restore icon: two overlapping squares */
        fb_drawline(max_x + 6, tby + 4, max_x + 13, tby + 4, COLOR_BLACK);
        fb_drawline(max_x + 6, tby + 5, max_x + 13, tby + 5, COLOR_BLACK);
        fb_drawline(max_x + 6, tby + 6, max_x + 6, tby + 7, COLOR_BLACK);
        fb_drawline(max_x + 13, tby + 6, max_x + 13, tby + 11, COLOR_BLACK);
        fb_drawline(max_x + 10, tby + 11, max_x + 13, tby + 11, COLOR_BLACK);

        fb_drawline(max_x + 2, tby + 8, max_x + 9, tby + 8, COLOR_BLACK);
        fb_drawline(max_x + 2, tby + 9, max_x + 9, tby + 9, COLOR_BLACK);
        fb_drawline(max_x + 2, tby + 10, max_x + 2, tby + 15, COLOR_BLACK);
        fb_drawline(max_x + 9, tby + 10, max_x + 9, tby + 15, COLOR_BLACK);
        fb_drawline(max_x + 2, tby + 15, max_x + 9, tby + 15, COLOR_BLACK);
    } else {
        /* Maximize icon: a single square */
        fb_drawline(max_x + 3, tby + 5, max_x + 12, tby + 5, COLOR_BLACK);
        fb_drawline(max_x + 3, tby + 6, max_x + 12, tby + 6, COLOR_BLACK);
        fb_drawline(max_x + 3, tby + 7, max_x + 3, tby + 14, COLOR_BLACK);
        fb_drawline(max_x + 12, tby + 7, max_x + 12, tby + 14, COLOR_BLACK);
        fb_drawline(max_x + 3, tby + 14, max_x + 12, tby + 14, COLOR_BLACK);
    }
    
    /* Minimize Button */
    fb_fillrect(min_x, tby + 2, btn_w, btn_w, COL_WIN_BG);
    draw_text(min_x + 4, tby + 2, "_", COLOR_BLACK);
    
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

void wm_render(void) {
    /* ---- 1. Desktop background ---- */
    if (desktop_bg_image) {
        extern uint16_t *fb_get_buffer(void);
        uint16_t *fbuf = fb_get_buffer();
        if (fbuf) {
            memcpy(fbuf, desktop_bg_image, FB_WIDTH * FB_HEIGHT * 2);
        }
    } else {
        fb_clear(COL_DESKTOP);
    }

    /* ---- 2a. App shortcut icons ---- */
    for (int i = 0; i < NUM_APPS; i++) {
        int ix = app_icons[i].x;
        int iy = app_icons[i].y;

        if (i == 0) {
            fb_fillrect(ix+4, iy+4, 56, 48, rgb565(80,80,90));
            fb_fillrect(ix+8, iy+8, 48, 40, rgb565(20,20,25));
            fb_fillrect(ix+12, iy+14, 6, 2, rgb565(80,240,80));
            fb_fillrect(ix+22, iy+14, 20, 2, rgb565(200,200,200));
            fb_fillrect(ix+12, iy+22, 6, 2, rgb565(80,240,80));
            fb_fillrect(ix+22, iy+22, 28, 2, rgb565(200,200,200));
            fb_fillrect(ix+12, iy+30, 6, 2, rgb565(80,240,80));
        } else if (i == 1) {
            fb_fillrect(ix+6, iy+10, 24, 10, rgb565(230,160,0));
            fb_fillrect(ix+14, iy+6, 32, 20, rgb565(245,245,255));
            fb_drawline(ix+18, iy+10, ix+34, iy+10, rgb565(200,200,220));
            fb_drawline(ix+18, iy+14, ix+40, iy+14, rgb565(200,200,220));
            fb_fillrect(ix+4, iy+20, 56, 30, rgb565(255,200,40));
            fb_fillrect(ix+4, iy+20, 56, 4, rgb565(255,225,100));
        } else if (i == 2) {
            fb_fillrect(ix+16, iy+12, 32, 34, rgb565(40,200,100));
            fb_fillrect(ix+10, iy+18, 44, 22, rgb565(40,200,100));
            fb_fillrect(ix+16, iy+12, 32, 4, rgb565(100,240,150));
            fb_fillrect(ix+20, iy+24, 6, 8, rgb565(20,20,30));
            fb_fillrect(ix+38, iy+24, 6, 8, rgb565(20,20,30));
            fb_fillrect(ix+22, iy+24, 2, 2, COLOR_WHITE);
            fb_fillrect(ix+40, iy+24, 2, 2, COLOR_WHITE);
        }
        
        draw_text(ix, iy + 56, app_icons[i].name, COLOR_WHITE);
    }
    
    /* ---- 2b. Filesystem icons ---- */
    extern volatile int doom_running;
    extern volatile int doom_loading;
    if (!desk_loaded && !doom_running && !doom_loading) desk_load_files();
    for (int i = 0; i < desk_count; i++) {
        if (!desk_files[i].valid) continue;
        int ix = desk_files[i].x;
        int iy = desk_files[i].y;
        if (iy + DESK_ICON_H > (int)FB_HEIGHT - TASKBAR_HEIGHT) break;

        if (desk_files[i].is_dir) {
            fb_fillrect(ix+8, iy+12, 24, 8, rgb565(230,160,0));
            fb_fillrect(ix+14, iy+8, 28, 16, rgb565(245,245,255));
            fb_drawline(ix+18, iy+12, ix+32, iy+12, rgb565(200,200,220));
            fb_drawline(ix+18, iy+16, ix+38, iy+16, rgb565(200,200,220));
            fb_fillrect(ix+6, iy+20, 52, 28, rgb565(255,200,40));
            fb_fillrect(ix+6, iy+20, 52, 3, rgb565(255,225,100));
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
        char lbl[10]; int j;
        for (j=0; j<8 && desk_files[i].name[j]; j++) lbl[j]=desk_files[i].name[j];
        lbl[j]='\0';
        draw_text(ix + 4, iy + 56, lbl, COLOR_WHITE);
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
    
    /* 3. Taskbar */
    int ty = FB_HEIGHT - TASKBAR_HEIGHT;
    fb_fillrect(0, ty, FB_WIDTH, TASKBAR_HEIGHT, COL_TASKBAR);
    fb_drawline(0, ty, FB_WIDTH, ty, COLOR_WHITE);
    fb_fillrect(4, ty + 4, 50, 20, rgb565(160, 160, 160));
    draw_text(10, ty + 6, "Start", COLOR_BLACK);
    
    int tb_idx = 0;
    for (int i = count - 1; i >= 0; i--) {
        if (arr[i]->state != WM_STATE_HIDDEN) {
            int bx = 60 + tb_idx * 100;
            if (bx + 100 > (int)(FB_WIDTH - 170)) break;
            uint16_t bcol = (arr[i]->state == WM_STATE_ACTIVE) ? rgb565(220, 220, 220) : rgb565(160, 160, 160);
            fb_fillrect(bx, ty + 4, 95, 20, bcol);
            char short_title[10];
            int j;
            for (j = 0; j < 9 && arr[i]->title[j] != '\0'; j++) {
                short_title[j] = arr[i]->title[j];
            }
            short_title[j] = '\0';
            draw_text(bx + 4, ty + 6, short_title, COLOR_BLACK);
            tb_idx++;
        }
    }
    
    /* Clock */
    extern volatile unsigned int tick_count;
    unsigned int secs = tick_count / 1000;
    int h = (secs / 3600) % 24;
    int m = (secs / 60) % 60;
    int s = secs % 60;
    char clock_str[10];
    clock_str[0] = '0' + (h / 10); clock_str[1] = '0' + (h % 10); clock_str[2] = ':';
    clock_str[3] = '0' + (m / 10); clock_str[4] = '0' + (m % 10); clock_str[5] = ':';
    clock_str[6] = '0' + (s / 10); clock_str[7] = '0' + (s % 10); clock_str[8] = '\0';
    draw_text(FB_WIDTH - 76, ty + 6, clock_str, COLOR_BLACK);
    
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
    draw_text(FB_WIDTH - 160, ty + 6, mem_str, COLOR_BLACK);
    
    /* Context Menu */
    if (ctx_menu.active) {
        fb_fillrect(ctx_menu.x, ctx_menu.y, 150, 150, rgb565(220, 220, 220));
        fb_drawline(ctx_menu.x, ctx_menu.y, ctx_menu.x + 149, ctx_menu.y, COLOR_WHITE);
        fb_drawline(ctx_menu.x, ctx_menu.y, ctx_menu.x, ctx_menu.y + 149, COLOR_WHITE);
        fb_drawline(ctx_menu.x + 149, ctx_menu.y, ctx_menu.x + 149, ctx_menu.y + 149, rgb565(100, 100, 100));
        fb_drawline(ctx_menu.x, ctx_menu.y + 149, ctx_menu.x + 149, ctx_menu.y + 149, rgb565(100, 100, 100));
        
        draw_text(ctx_menu.x + 10, ctx_menu.y + 8, "New Terminal", COLOR_BLACK);
        fb_drawline(ctx_menu.x + 5, ctx_menu.y + 30, ctx_menu.x + 145, ctx_menu.y + 30, rgb565(160, 160, 160));
        draw_text(ctx_menu.x + 10, ctx_menu.y + 38, "File Manager", COLOR_BLACK);
        fb_drawline(ctx_menu.x + 5, ctx_menu.y + 60, ctx_menu.x + 145, ctx_menu.y + 60, rgb565(160, 160, 160));
        draw_text(ctx_menu.x + 10, ctx_menu.y + 68, "New Folder", COLOR_BLACK);
        fb_drawline(ctx_menu.x + 5, ctx_menu.y + 90, ctx_menu.x + 145, ctx_menu.y + 90, rgb565(160, 160, 160));
        draw_text(ctx_menu.x + 10, ctx_menu.y + 98, "New File", COLOR_BLACK);
        fb_drawline(ctx_menu.x + 5, ctx_menu.y + 120, ctx_menu.x + 145, ctx_menu.y + 120, rgb565(160, 160, 160));
        draw_text(ctx_menu.x + 10, ctx_menu.y + 128, "Change BG", COLOR_BLACK);
    }
    
    /* Start Menu */
    if (start_menu_active) {
        int sm_x = 0;
        int sm_y = FB_HEIGHT - TASKBAR_HEIGHT - 280;
        int sm_w = 140;
        int sm_h = 280;
        
        fb_fillrect(sm_x, sm_y, sm_w, sm_h, rgb565(40,40,45));
        fb_drawline(sm_x, sm_y, sm_x + sm_w, sm_y, rgb565(100,100,120));
        fb_drawline(sm_x + sm_w, sm_y, sm_x + sm_w, sm_y + sm_h, rgb565(20,20,25));
        
        fb_fillrect(sm_x, sm_y, sm_w, 24, rgb565(60,120,200));
        draw_text(sm_x + 10, sm_y + 4, "Apps", COLOR_WHITE);
        
        int ix1 = sm_x + 10, iy1 = sm_y + 34;
        fb_fillrect(ix1, iy1, 24, 24, rgb565(240,240,245));
        fb_fillrect(ix1+4, iy1+14, 4, 6, rgb565(255,80,80));
        fb_fillrect(ix1+10, iy1+10, 4, 10, rgb565(255,180,40));
        fb_fillrect(ix1+16, iy1+6, 4, 14, rgb565(60,200,120));
        draw_text(ix1 + 32, iy1 + 4, "Task Mgr", COLOR_WHITE);
        
        int ix2 = sm_x + 10, iy2 = sm_y + 74;
        fb_fillrect(ix2, iy2, 24, 24, rgb565(30,40,30));
        fb_fillrect(ix2+4, iy2+4, 4, 4, rgb565(80,220,80));
        fb_fillrect(ix2+10, iy2+4, 4, 4, rgb565(80,220,80));
        fb_fillrect(ix2+16, iy2+4, 4, 4, rgb565(80,220,80));
        fb_fillrect(ix2+16, iy2+10, 4, 4, rgb565(80,220,80));
        fb_fillrect(ix2+16, iy2+16, 4, 4, rgb565(120,255,120));
        draw_text(ix2 + 32, iy2 + 4, "Snake", COLOR_WHITE);

        int ix3 = sm_x + 10, iy3 = sm_y + 114;
        fb_fillrect(ix3, iy3, 24, 24, rgb565(150,110,60));
        fb_drawline(ix3, iy3, ix3+23, iy3+23, rgb565(100,60,30));
        fb_drawline(ix3+23, iy3, ix3, iy3+23, rgb565(100,60,30));
        draw_text(ix3 + 32, iy3 + 4, "Sokoban", COLOR_WHITE);

        int ix4 = sm_x + 10, iy4 = sm_y + 154;
        fb_fillrect(ix4, iy4, 24, 24, rgb565(200,200,200));
        fb_fillrect(ix4+4, iy4+4, 16, 4, rgb565(100,100,100)); // screen
        fb_fillrect(ix4+4, iy4+10, 4, 4, rgb565(50,50,50)); // keys
        fb_fillrect(ix4+10, iy4+10, 4, 4, rgb565(50,50,50));
        fb_fillrect(ix4+16, iy4+10, 4, 4, rgb565(50,50,50));
        draw_text(ix4 + 32, iy4 + 4, "Calc", COLOR_WHITE);
        
        int ix5 = sm_x + 10, iy5 = sm_y + 194;
        fb_fillrect(ix5, iy5, 24, 24, rgb565(50,50,150));
        fb_fillrect(ix5+8, iy5+4, 8, 4, COLOR_WHITE); // 'i' dot
        fb_fillrect(ix5+8, iy5+10, 8, 10, COLOR_WHITE); // 'i' stem
        draw_text(ix5 + 32, iy5 + 4, "SysInfo", COLOR_WHITE);

        int ix6 = sm_x + 10, iy6 = sm_y + 234;
        fb_fillrect(ix6, iy6, 24, 24, rgb565(80,30,80));
        fb_fillrect(ix6+4, iy6+4, 16, 16, rgb565(200,200,200));
        fb_fillrect(ix6+6, iy6+6, 4, 4, rgb565(0,0,0));
        fb_fillrect(ix6+14, iy6+6, 4, 4, rgb565(0,0,0));
        draw_text(ix6 + 32, iy6 + 4, "MemView", COLOR_WHITE);
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
