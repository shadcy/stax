/* ============================================================================
 * TIOS — wm.c
 * Compositing Window Manager
 * ============================================================================ */

#include "wm.h"
#include "framebuffer.h"
#include "mouse.h"
#include "string.h"
#include "heap.h"
#include "gfx_console.h"
#include "doom.h"
#include "app_file_manager.h"
#include "app_terminal.h"
#include "app_editor.h"
#include "fatfs/ff.h"

#define TITLEBAR_HEIGHT 20
#define BORDER_WIDTH    2
#define TASKBAR_HEIGHT  28

#define RGB565_C(r, g, b) (((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3))
static int bg_color_idx = 0;
static uint16_t bg_colors[] = {
    RGB565_C(58, 110, 165),  /* Classic Blue (Windows NT/98 default) */
    RGB565_C(0, 128, 128),   /* Default Teal (Windows 95 Classic) */
    RGB565_C(0, 0, 0),       /* Black */
    RGB565_C(128, 0, 0),     /* Dark Red */
    RGB565_C(128, 128, 128)  /* Gray */
};
#define COL_DESKTOP     (bg_colors[bg_color_idx])
#define COL_WIN_BG      rgb565(192, 192, 192)
#define COL_WIN_TITLE   rgb565(0, 0, 128)
#define COL_WIN_TITLE_TXT COLOR_WHITE
#define COL_WIN_BORDER_LIGHT COLOR_WHITE
#define COL_WIN_BORDER_DARK  rgb565(128, 128, 128)
#define COL_TASKBAR     rgb565(192, 192, 192)

window_t *window_list = NULL;
static int next_id = 1;

static window_t *drag_win = NULL;
static int drag_off_x = 0;
static int drag_off_y = 0;
static int prev_mouse_b = 0;
static window_t *focused_window = NULL;

static void wm_bring_to_front(window_t *win);

static context_menu_t ctx_menu = {0, 0, 0};

/* Desktop App Shortcuts */
#define ICON_W       64
#define ICON_H       64
#define ICON_SPACING 80

typedef struct {
    int id;
    int x, y;
    const char *name;
} app_icon_t;

/* We removed Task Mgr (3) and Snake (4) from the desktop */
static app_icon_t app_icons[] = {
    {0, 10, 10, "Boot Log"},
    {1, 10, 90, "File Mgr"},
    {2, 10, 170, "sh** slime"}
};
#define NUM_APPS 3

/* Start Menu state */
static int start_menu_active = 0;

/* Desktop Filesystem Icons (root dir, right side of desktop) */
#define DESK_MAX        20
#define DESK_REFRESH_MS 3000
#define DESK_START_X    100   /* x offset where filesystem icons begin    */
#define DESK_ICON_W     74    /* column stride (icon 64px + 10px gap)     */
#define DESK_ICON_H     80    /* row stride                                */

typedef struct {
    char name[16];
    int  is_dir;
    int  x, y;
    int  valid;
} desk_file_t;

static desk_file_t desk_files[DESK_MAX];
static int         desk_count    = 0;
static int         desk_loaded   = 0;
static int         desk_refresh  = 0;   /* ms accumulator */

static void desk_load_files(void) {
    DIR dir; FILINFO fno;
    
    /* Mark all existing as invalid */
    for (int i = 0; i < DESK_MAX; i++) desk_files[i].valid = 0;
    
    if (f_opendir(&dir, ".") == FR_OK) {
        int idx = 0;
        while (idx < DESK_MAX) {
            if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0]==0) break;
            if (fno.fname[0]=='.') continue;
            
            /* Check if this file is already in the list to preserve x, y */
            int found = -1;
            for (int j = 0; j < desk_count; j++) {
                int k=0, match=1;
                while (fno.fname[k] || desk_files[j].name[k]) {
                    if (fno.fname[k] != desk_files[j].name[k]) { match=0; break; }
                    k++;
                }
                if (match) { found = j; break; }
            }
            
            if (found >= 0) {
                desk_files[found].valid = 1;
                desk_files[found].is_dir = (fno.fattrib & AM_DIR) ? 1 : 0;
            } else {
                /* Add new */
                int slot = desk_count;
                if (slot < DESK_MAX) {
                    int k=0; while(k<15 && fno.fname[k]) { desk_files[slot].name[k]=fno.fname[k]; k++; }
                    desk_files[slot].name[k] = '\0';
                    desk_files[slot].is_dir = (fno.fattrib & AM_DIR) ? 1 : 0;
                    
                    int cols = (FB_WIDTH - DESK_START_X - 10) / DESK_ICON_W;
                    if (cols < 1) cols = 1;
                    desk_files[slot].x = DESK_START_X + (slot % cols) * DESK_ICON_W;
                    desk_files[slot].y = 10 + (slot / cols) * DESK_ICON_H;
                    desk_files[slot].valid = 1;
                    desk_count++;
                }
            }
            idx++;
        }
        f_closedir(&dir);
        
        /* Compact array to remove invalid entries */
        int w = 0;
        for (int r = 0; r < desk_count; r++) {
            if (desk_files[r].valid) {
                if (w != r) desk_files[w] = desk_files[r];
                w++;
            }
        }
        desk_count = w;
    }
    desk_loaded = 1;
}

/* For dragging icons */
static int drag_type = -1; /* 0 = app, 1 = file, -1 = none */
static int drag_idx = -1;
static int drag_moved = 0;

/* Simple 11x16 bitmapped arrow cursor */
#define CURSOR_W 11
#define CURSOR_H 16
static const char cursor_bitmap[CURSOR_H][CURSOR_W] = {
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

void wm_init(void) {
    fb_set_double_buffering(1);
}

window_t *wm_add_window(int x, int y, int w, int h, const char *title, void (*draw_cb)(window_t*, int, int, int, int)) {
    window_t *win = (window_t*)kmalloc(sizeof(window_t));
    if (!win) return NULL;
    
    win->id = next_id++;
    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    
    int i;
    for (i = 0; i < 31 && title[i] != '\0'; i++) {
        win->title[i] = title[i];
    }
    win->title[i] = '\0';
    
    win->state = WM_STATE_ACTIVE;
    win->draw_client = draw_cb;
    
    win->update_client = NULL;
    win->key_event = NULL;
    win->mouse_click = NULL;
    win->path[0] = '\0';
    win->app_data = NULL;
    win->next = window_list;
    window_list = win;
    focused_window = win;
    
    return win;
}

void wm_close_window(window_t *win)
{
    if (!win) return;
    win->state = WM_STATE_HIDDEN;
    win->key_event = NULL;
    win->update_client = NULL;
    if (focused_window == win)
        focused_window = NULL;
}

void wm_focus_shell(void)
{
    window_t *curr = window_list;
    while (curr) {
        if (strcmp(curr->title, "Boot Log") == 0) {
            wm_bring_to_front(curr);
            focused_window = curr;
            return;
        }
        curr = curr->next;
    }
    focused_window = NULL;
}

int wm_dispatch_key(char c) {
    if (c == '\t') {
        if (window_list && window_list->next) {
            window_t *last = window_list;
            while (last->next) last = last->next;
            window_t *old_head = window_list;
            window_list = window_list->next;
            old_head->next = NULL;
            last->next = old_head;
            focused_window = window_list;
            
            if (focused_window->state == WM_STATE_MINIMIZED) {
                focused_window->state = WM_STATE_ACTIVE;
            }
        }
        return 1;
    }
    if (focused_window
        && focused_window->state == WM_STATE_ACTIVE
        && focused_window->key_event) {
        focused_window->key_event(focused_window, c);
        return 1;
    }
    return 0;
}

static void wm_bring_to_front(window_t *win) {
    if (!win || window_list == win) return;
    
    window_t *prev = NULL;
    window_t *curr = window_list;
    while (curr && curr != win) {
        prev = curr;
        curr = curr->next;
    }
    if (curr) {
        prev->next = curr->next;
        curr->next = window_list;
        window_list = curr;
    }
}

static void draw_window(window_t *win) {
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
    
    /* Buttons (Minimize and Close) */
    int btn_w = 16;
    int close_x = wx + ww - BORDER_WIDTH - btn_w - 2;
    int min_x = close_x - btn_w - 2;
    
    /* Close Button */
    fb_fillrect(close_x, tby + 2, btn_w, btn_w, COL_WIN_BG);
    draw_text(close_x + 4, tby + 2, "X", COLOR_BLACK);
    
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

void wm_update(void) {
    extern volatile unsigned int tick_count;
    static unsigned int last_tick = 0;
    unsigned int current_tick = tick_count;
    int dt_ms = current_tick - last_tick;
    if (dt_ms > 0) {
        window_t *curr = window_list;
        while (curr) {
            if (curr->state != WM_STATE_HIDDEN && curr->update_client) {
                curr->update_client(curr, dt_ms);
            }
            curr = curr->next;
        }
        last_tick = current_tick;
        /* Desktop filesystem icons: periodic refresh */
        extern volatile int doom_running;
        extern volatile int doom_loading;
        desk_refresh += dt_ms;
        if (desk_refresh >= DESK_REFRESH_MS) {
            if (!doom_running && !doom_loading) desk_loaded = 0;
            desk_refresh = 0;
        }
    }
    
    int mb = mouse_buttons;
    int mx = mouse_x;
    int my = mouse_y;
    
    int right_pressed = (mb & 2) && !(prev_mouse_b & 2);
    int pressed = (mb & 1) && !(prev_mouse_b & 1);
    int released = !(mb & 1) && (prev_mouse_b & 1);
    
    if (pressed || right_pressed) {
        /* Close context menu if open */
        if (ctx_menu.active) {
            /* Check if clicking inside context menu */
            if (pressed && mx >= ctx_menu.x && mx < ctx_menu.x + 150 && 
                my >= ctx_menu.y && my < ctx_menu.y + 120) {
                
                int item = (my - ctx_menu.y) / 30;
                if (item == 0) {
                    /* New Terminal — fresh per-window terminal with own buffer */
                    window_t *tw = wm_add_window(80, 60, 520, 340, "Terminal", terminal_draw_window);
                    if (tw) tw->key_event = terminal_key_event;
                } else if (item == 1) {
                    /* New File Manager */
                    window_t *fw = wm_add_window(150, 150, 420, 320, "File Manager", file_manager_draw_window);
                    if (fw) { fw->mouse_click = file_manager_click; fw->update_client = file_manager_update; }
                } else if (item == 2) {
                    /* New Folder */
                    f_mkdir("NEWDIR");
                    file_manager_refresh();
                } else if (item == 3) {
                    /* New File */
                    FIL f;
                    if (f_open(&f, "NEWFILE.TXT", FA_CREATE_NEW | FA_WRITE) == FR_OK) {
                        f_close(&f);
                    }
                } else if (item == 4) {
                    /* Change Background */
                    bg_color_idx = (bg_color_idx + 1) % 5;
                }
            }
            ctx_menu.active = 0;
            if (!right_pressed) goto update_done;
        }

        /* Taskbar hit test (simplified) */
        if (my >= (int)(FB_HEIGHT - TASKBAR_HEIGHT)) {
            /* Check if clicking a taskbar item to restore */
            if (pressed) {
                if (mx >= 0 && mx < 60) {
                    /* Start button clicked */
                    start_menu_active = !start_menu_active;
                } else {
                    start_menu_active = 0;
                int tb_idx = 0;
                window_t *curr = window_list;
                while (curr) {
                    if (curr->state != WM_STATE_HIDDEN) {
                        int item_x = 60 + tb_idx * 100;
                        if (mx >= item_x && mx < item_x + 95) {
                            if (curr->state == WM_STATE_MINIMIZED) {
                                curr->state = WM_STATE_ACTIVE;
                                wm_bring_to_front(curr);
                            } else {
                                curr->state = WM_STATE_MINIMIZED;
                            }
                            break;
                        }
                        tb_idx++;
                    }
                    curr = curr->next;
                }
                }
            }
            drag_win = NULL;
        } else {
            /* Close start menu if clicked outside */
            if (pressed && start_menu_active) {
                /* check if click is inside start menu */
                int sm_x = 0;
                int sm_y = FB_HEIGHT - TASKBAR_HEIGHT - 280;
                int sm_w = 140;
                int sm_h = 280;
                if (mx >= sm_x && mx < sm_x + sm_w && my >= sm_y && my < sm_y + sm_h) {
                    /* Handle Start Menu Clicks */
                    int item = (my - sm_y - 24) / 40;
                    if (item == 0) {
                        /* Task Manager */
                        extern void taskmgr_draw_window(struct window *win,int cx,int cy,int cw,int ch);
                        wm_add_window(100,100,400,300,"Task Manager",taskmgr_draw_window);
                        start_menu_active = 0;
                    } else if (item == 1) {
                        /* Snake */
                        extern void terminal_draw_window(struct window *win,int cx,int cy,int cw,int ch);
                        extern void terminal_key_event(struct window *win,char c);
                        window_t *tw=wm_add_window(80,60,520,340,"Snake",terminal_draw_window);
                        if (tw) tw->key_event=terminal_key_event;
                        start_menu_active = 0;
                    } else if (item == 2) {
                        /* Sokoban */
                        extern void cmd_sokoban(int, char**);
                        cmd_sokoban(0, 0);
                        start_menu_active = 0;
                    } else if (item == 3) {
                        /* Calculator */
                        extern void calculator_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                        extern void calculator_mouse_click(struct window *win, int mx, int my, int btn);
                        window_t *cw = wm_add_window(200, 100, 210, 260, "Calculator", calculator_draw_window);
                        if (cw) cw->mouse_click = calculator_mouse_click;
                        start_menu_active = 0;
                    } else if (item == 4) {
                        /* SysInfo */
                        extern void sysinfo_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                        wm_add_window(150, 150, 300, 260, "System Info", sysinfo_draw_window);
                        start_menu_active = 0;
                    } else if (item == 5) {
                        /* MemView */
                        extern void memview_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                        extern void memview_key_event(struct window *win, char c);
                        window_t *mw = wm_add_window(100, 80, 420, 280, "Mem Viewer", memview_draw_window);
                        if (mw) mw->key_event = memview_key_event;
                        start_menu_active = 0;
                    }
                    /* End hit-testing to prevent clicking windows behind it */
                    goto update_done;
                } else {
                    start_menu_active = 0;
                }
            }
            
            /* Window hit test (front to back) */
            window_t *curr = window_list;
            int hit_window = 0;
            while (curr) {
                if (curr->state == WM_STATE_ACTIVE &&
                    mx >= curr->x && mx < curr->x + curr->width &&
                    my >= curr->y && my < curr->y + curr->height) {
                    
                    hit_window = 1;
                    
                    if (pressed) {
                        wm_bring_to_front(curr);
                        focused_window = curr;
                        
                        /* Check buttons */
                        int tby = curr->y + BORDER_WIDTH;
                        int btn_w = 16;
                        int close_x = curr->x + curr->width - BORDER_WIDTH - btn_w - 2;
                        int min_x = close_x - btn_w - 2;
                        
                        if (my >= tby + 2 && my < tby + 2 + btn_w) {
                            if (mx >= close_x && mx < close_x + btn_w) {
                                if (curr->app_data == DOOM_WIN_MARKER) {
                                    doom_force_cleanup();
                                    if (focused_window == curr)
                                        focused_window = NULL;
                                    break;
                                }
                                /* Auto-save if this is an editor window */
                                extern void editor_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                                if (curr->draw_client == editor_draw_window) {
                                    editor_autosave(curr);
                                    file_manager_refresh(); /* refresh FM so renamed/saved files show */
                                }
                                curr->state = WM_STATE_HIDDEN; /* Close */
                                extern void image_viewer_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                                if (curr->draw_client == image_viewer_draw_window && curr->app_data && curr->app_data != (void*)1) {
                                    extern void kfree(void*);
                                    kfree(curr->app_data);
                                    curr->app_data = NULL;
                                }
                                if (focused_window == curr) focused_window = NULL;
                                break;
                            }
                            if (mx >= min_x && mx < min_x + btn_w) {
                                curr->state = WM_STATE_MINIMIZED;
                                if (focused_window == curr) focused_window = NULL;
                                break;
                            }
                        }
                        
                        /* Check Titlebar drag / double click */
                        if (my >= curr->y && my < curr->y + BORDER_WIDTH + TITLEBAR_HEIGHT) {
                            extern volatile unsigned int tick_count;
                            static unsigned int last_click = 0;
                            static window_t *last_win = NULL;
                            if (last_win == curr && (tick_count - last_click) < 300) {
                                if (curr->is_maximized) {
                                    curr->x = curr->saved_x;
                                    curr->y = curr->saved_y;
                                    curr->width = curr->saved_width;
                                    curr->height = curr->saved_height;
                                    curr->is_maximized = 0;
                                } else {
                                    curr->saved_x = curr->x;
                                    curr->saved_y = curr->y;
                                    curr->saved_width = curr->width;
                                    curr->saved_height = curr->height;
                                    curr->x = 0;
                                    curr->y = 0;
                                    curr->width = FB_WIDTH;
                                    curr->height = FB_HEIGHT - TASKBAR_HEIGHT;
                                    curr->is_maximized = 1;
                                }
                                last_click = 0;
                            } else {
                                drag_win = curr;
                                drag_off_x = mx - curr->x;
                                drag_off_y = my - curr->y;
                                last_click = tick_count;
                                last_win = curr;
                            }
                        } else {
                            /* Client area click */
                            if (curr->mouse_click) {
                                curr->mouse_click(curr, mx - curr->x, my - curr->y - BORDER_WIDTH - TITLEBAR_HEIGHT, mb & 3);
                            }
                        }
                    } else if (right_pressed) {
                        focused_window = curr;
                        wm_bring_to_front(curr);
                        /* Right click inside client area */
                        if (curr->mouse_click && my >= curr->y + BORDER_WIDTH + TITLEBAR_HEIGHT) {
                            curr->mouse_click(curr, mx - curr->x, my - curr->y - BORDER_WIDTH - TITLEBAR_HEIGHT, mb & 3);
                        }
                    }
                    break;
                }
                curr = curr->next;
            }
            
            /* Desktop clicks */
            if (!hit_window) {
                if (pressed || right_pressed) focused_window = NULL;
                if (right_pressed) {
                    ctx_menu.active = 1;
                    ctx_menu.x = mx;
                    ctx_menu.y = my;
                    if (ctx_menu.x + 150 > (int)FB_WIDTH) ctx_menu.x = FB_WIDTH - 150;
                    if (ctx_menu.y + 150 > (int)(FB_HEIGHT - TASKBAR_HEIGHT)) ctx_menu.y = FB_HEIGHT - TASKBAR_HEIGHT - 150;
                } else if (pressed) {
                    /* Desktop Icons hit test */
                    for (int i = 0; i < NUM_APPS; i++) {
                        int ix = app_icons[i].x;
                        int iy = app_icons[i].y;
                        if (mx >= ix && mx < ix + ICON_W && my >= iy && my < iy + ICON_H) {
                            drag_type = 0;
                            drag_idx = i;
                            drag_off_x = mx - ix;
                            drag_off_y = my - iy;
                            drag_moved = 0;
                            goto desktop_hit_done;
                        }
                    }
                    /* Filesystem Icons hit test (right side) */
                    if (!desk_loaded) desk_load_files();
                    for (int i = 0; i < desk_count; i++) {
                        if (!desk_files[i].valid) continue;
                        int ix = desk_files[i].x;
                        int iy = desk_files[i].y;
                        if (mx >= ix && mx < ix + 64 && my >= iy && my < iy + 64) {
                            drag_type = 1;
                            drag_idx = i;
                            drag_off_x = mx - ix;
                            drag_off_y = my - iy;
                            drag_moved = 0;
                            goto desktop_hit_done;
                        }
                    }
desktop_hit_done:
                    ;
                }
            }
        }

    } else if (released) {
        drag_win = NULL;
        /* If we clicked an icon but didn't drag it, launch it */
        if (drag_type != -1 && !drag_moved) {
            if (drag_type == 0 && drag_idx >= 0 && drag_idx < NUM_APPS) {
                int i = drag_idx;
                if (app_icons[i].id == 0) {
                    wm_add_window(40,40,560,360,"Boot Log",gfx_console_draw_window);
                } else if (app_icons[i].id == 1) {
                    window_t *fw=wm_add_window(60,60,420,320,"File Manager",file_manager_draw_window);
                    if (fw) { fw->mouse_click=file_manager_click; fw->update_client=file_manager_update; }
                } else if (app_icons[i].id == 2) {
                    extern void cmd_slime(int, char**);
                    cmd_slime(0,0);
                }
            } else if (drag_type == 1 && drag_idx >= 0 && drag_idx < desk_count) {
                int i = drag_idx;
                if (desk_files[i].is_dir) {
                    /* Open File Manager */
                    window_t *fw=wm_add_window(60,60,420,320,desk_files[i].name,file_manager_draw_window);
                    if (fw) {
                        fw->mouse_click=file_manager_click;
                        fw->update_client=file_manager_update;
                        int k=0; while(desk_files[i].name[k]) { fw->path[k]=desk_files[i].name[k]; k++; }
                        fw->path[k]='\0';
                    }
                } else {
                    /* Open Editor if TXT */
                    int nlen=0; while(desk_files[i].name[nlen]) nlen++;
                    if (nlen>4 &&
                        (desk_files[i].name[nlen-3]=='T'||desk_files[i].name[nlen-3]=='t') &&
                        (desk_files[i].name[nlen-2]=='X'||desk_files[i].name[nlen-2]=='x') &&
                        (desk_files[i].name[nlen-1]=='T'||desk_files[i].name[nlen-1]=='t')) {
                        
                        extern void editor_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                        extern void editor_key_event(struct window *win, char c);
                        window_t *nw = wm_add_window(80, 80, 500, 350, desk_files[i].name, editor_draw_window);
                        if (nw) {
                            nw->key_event = editor_key_event;
                            int k=0; while(desk_files[i].name[k]) { nw->path[k]=desk_files[i].name[k]; k++; }
                            nw->path[k]='\0';
                        }
                    } else if (nlen>4 &&
                        (desk_files[i].name[nlen-3]=='B'||desk_files[i].name[nlen-3]=='b') &&
                        (desk_files[i].name[nlen-2]=='M'||desk_files[i].name[nlen-2]=='m') &&
                        (desk_files[i].name[nlen-1]=='P'||desk_files[i].name[nlen-1]=='p')) {
                        
                        extern void image_viewer_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                        window_t *nw = wm_add_window(100, 100, 320, 240, desk_files[i].name, image_viewer_draw_window);
                        if (nw) {
                            int k=0; while(desk_files[i].name[k]) { nw->path[k]=desk_files[i].name[k]; k++; }
                            nw->path[k]='\0';
                        }
                    }
                }
            }
        } else if (drag_type != -1 && drag_moved) {
            /* Snap to grid */
            if (drag_type == 0 && drag_idx >= 0 && drag_idx < NUM_APPS) {
                int nx = ((app_icons[drag_idx].x - 10 + ICON_SPACING/2) / ICON_SPACING) * ICON_SPACING + 10;
                int ny = ((app_icons[drag_idx].y - 10 + ICON_SPACING/2) / ICON_SPACING) * ICON_SPACING + 10;
                app_icons[drag_idx].x = nx < 10 ? 10 : nx;
                app_icons[drag_idx].y = ny < 10 ? 10 : ny;
            } else if (drag_type == 1 && drag_idx >= 0 && drag_idx < desk_count) {
                int nx = ((desk_files[drag_idx].x - DESK_START_X + DESK_ICON_W/2) / DESK_ICON_W) * DESK_ICON_W + DESK_START_X;
                int ny = ((desk_files[drag_idx].y - 10 + DESK_ICON_H/2) / DESK_ICON_H) * DESK_ICON_H + 10;
                desk_files[drag_idx].x = nx < DESK_START_X ? DESK_START_X : nx;
                desk_files[drag_idx].y = ny < 10 ? 10 : ny;
            }
        }
        drag_type = -1;
    }
    
    if (mb & 1) {
        if (drag_win) {
            drag_win->x = mx - drag_off_x;
            drag_win->y = my - drag_off_y;
        } else if (drag_type == 0 && drag_idx >= 0 && drag_idx < NUM_APPS) {
            int old_x = app_icons[drag_idx].x;
            int old_y = app_icons[drag_idx].y;
            app_icons[drag_idx].x = mx - drag_off_x;
            app_icons[drag_idx].y = my - drag_off_y;
            if (old_x != app_icons[drag_idx].x || old_y != app_icons[drag_idx].y) drag_moved = 1;
        } else if (drag_type == 1 && drag_idx >= 0 && drag_idx < desk_count) {
            int old_x = desk_files[drag_idx].x;
            int old_y = desk_files[drag_idx].y;
            desk_files[drag_idx].x = mx - drag_off_x;
            desk_files[drag_idx].y = my - drag_off_y;
            if (old_x != desk_files[drag_idx].x || old_y != desk_files[drag_idx].y) drag_moved = 1;
        }
    }
    
update_done:
    prev_mouse_b = mb;
}

static uint16_t *desktop_bg_image = NULL;

extern uint16_t *bmp_load(const char *filename, int *out_w, int *out_h);
void wm_load_background(const char *filename) {
    if (desktop_bg_image) {
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
    /* Draw distinct per-app icons */
    for (int i = 0; i < NUM_APPS; i++) {
        int ix = app_icons[i].x;
        int iy = app_icons[i].y;

        /* Icon box background (transparent/desktop color for modern look, removing sunken border) */
        
        if (i == 0) {
            /* Boot Log — Modern terminal icon */
            /* Frame */
            fb_fillrect(ix+4, iy+4, 56, 48, rgb565(80,80,90));
            /* Screen */
            fb_fillrect(ix+8, iy+8, 48, 40, rgb565(20,20,25));
            /* Prompt and text */
            fb_fillrect(ix+12, iy+14, 6, 2, rgb565(80,240,80));
            fb_fillrect(ix+22, iy+14, 20, 2, rgb565(200,200,200));
            fb_fillrect(ix+12, iy+22, 6, 2, rgb565(80,240,80));
            fb_fillrect(ix+22, iy+22, 28, 2, rgb565(200,200,200));
            fb_fillrect(ix+12, iy+30, 6, 2, rgb565(80,240,80));
        } else if (i == 1) {
            /* File Mgr — Modern Folder with Document */
            /* Back tab */
            fb_fillrect(ix+6, iy+10, 24, 10, rgb565(230,160,0));
            /* White document sticking out */
            fb_fillrect(ix+14, iy+6, 32, 20, rgb565(245,245,255));
            fb_drawline(ix+18, iy+10, ix+34, iy+10, rgb565(200,200,220));
            fb_drawline(ix+18, iy+14, ix+40, iy+14, rgb565(200,200,220));
            /* Front folder body */
            fb_fillrect(ix+4, iy+20, 56, 30, rgb565(255,200,40));
            /* Highlight on front */
            fb_fillrect(ix+4, iy+20, 56, 4, rgb565(255,225,100));
        } else if (i == 2) {
            /* Slime — Cute pixel blob */
            fb_fillrect(ix+16, iy+12, 32, 34, rgb565(40,200,100));
            fb_fillrect(ix+10, iy+18, 44, 22, rgb565(40,200,100));
            /* Highlight */
            fb_fillrect(ix+16, iy+12, 32, 4, rgb565(100,240,150));
            /* Eyes */
            fb_fillrect(ix+20, iy+24, 6, 8, rgb565(20,20,30));
            fb_fillrect(ix+38, iy+24, 6, 8, rgb565(20,20,30));
            /* Eye glints */
            fb_fillrect(ix+22, iy+24, 2, 2, COLOR_WHITE);
            fb_fillrect(ix+40, iy+24, 2, 2, COLOR_WHITE);
        }
        
        /* Label */
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
            /* Folder icon — Modern Folder */
            /* Back tab */
            fb_fillrect(ix+8, iy+12, 24, 8, rgb565(230,160,0));
            /* Document sticking out */
            fb_fillrect(ix+14, iy+8, 28, 16, rgb565(245,245,255));
            fb_drawline(ix+18, iy+12, ix+32, iy+12, rgb565(200,200,220));
            fb_drawline(ix+18, iy+16, ix+38, iy+16, rgb565(200,200,220));
            /* Front folder body */
            fb_fillrect(ix+6, iy+20, 52, 28, rgb565(255,200,40));
            /* Highlight on front */
            fb_fillrect(ix+6, iy+20, 52, 3, rgb565(255,225,100));
        } else {
            /* Check extension for icon colour */
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
                                       
            /* Page body */
            fb_fillrect(ix+14, iy+6, 32, 42, page_col);
            
            /* Border */
            fb_drawline(ix+14, iy+6, ix+34, iy+6, border);
            fb_drawline(ix+14, iy+6, ix+14, iy+47, border);
            fb_drawline(ix+14, iy+47, ix+45, iy+47, border);
            fb_drawline(ix+45, iy+17, ix+45, iy+47, border);
            fb_drawline(ix+34, iy+6, ix+45, iy+17, border);
            
            /* Folded corner (shadow and inner part) */
            fb_fillrect(ix+35, iy+7, 10, 10, rgb565(210,210,220));
            fb_drawline(ix+34, iy+17, ix+45, iy+17, border);
            fb_drawline(ix+34, iy+6, ix+34, iy+17, border);
            /* Content lines */
            if (is_t) {
                for (int l=0;l<4;l++) fb_fillrect(ix+18, iy+20+l*6, 22, 2, border);
            } else if (is_b) {
                fb_fillrect(ix+18, iy+22, 22, 3, rgb565(180,180,190));
                fb_fillrect(ix+18, iy+28, 14, 3, rgb565(180,180,190));
                fb_fillrect(ix+18, iy+34, 18, 3, rgb565(180,180,190));
            }
        }
        /* Selection border placeholder (no selection on desktop files yet) */
        /* Label — truncate to 8 chars */
        char lbl[10]; int j;
        for (j=0; j<8 && desk_files[i].name[j]; j++) lbl[j]=desk_files[i].name[j];
        lbl[j]='\0';
        draw_text(ix + 4, iy + 56, lbl, COLOR_WHITE);
    }
    /* To draw back-to-front from a forward list, we need to reverse traverse or gather in array */
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
    
    /* Taskbar buttons for windows */
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
        
        /* Items */
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
        
        /* Background */
        fb_fillrect(sm_x, sm_y, sm_w, sm_h, rgb565(40,40,45));
        /* Border */
        fb_drawline(sm_x, sm_y, sm_x + sm_w, sm_y, rgb565(100,100,120));
        fb_drawline(sm_x + sm_w, sm_y, sm_x + sm_w, sm_y + sm_h, rgb565(20,20,25));
        
        /* Title */
        fb_fillrect(sm_x, sm_y, sm_w, 24, rgb565(60,120,200));
        draw_text(sm_x + 10, sm_y + 4, "Apps", COLOR_WHITE);
        
        /* Items */
        /* Task Manager */
        int ix1 = sm_x + 10, iy1 = sm_y + 34;
        fb_fillrect(ix1, iy1, 24, 24, rgb565(240,240,245));
        fb_fillrect(ix1+4, iy1+14, 4, 6, rgb565(255,80,80));
        fb_fillrect(ix1+10, iy1+10, 4, 10, rgb565(255,180,40));
        fb_fillrect(ix1+16, iy1+6, 4, 14, rgb565(60,200,120));
        draw_text(ix1 + 32, iy1 + 4, "Task Mgr", COLOR_WHITE);
        
        /* Snake */
        int ix2 = sm_x + 10, iy2 = sm_y + 74;
        fb_fillrect(ix2, iy2, 24, 24, rgb565(30,40,30));
        fb_fillrect(ix2+4, iy2+4, 4, 4, rgb565(80,220,80));
        fb_fillrect(ix2+10, iy2+4, 4, 4, rgb565(80,220,80));
        fb_fillrect(ix2+16, iy2+4, 4, 4, rgb565(80,220,80));
        fb_fillrect(ix2+16, iy2+10, 4, 4, rgb565(80,220,80));
        fb_fillrect(ix2+16, iy2+16, 4, 4, rgb565(120,255,120));
        draw_text(ix2 + 32, iy2 + 4, "Snake", COLOR_WHITE);

        /* Sokoban */
        int ix3 = sm_x + 10, iy3 = sm_y + 114;
        fb_fillrect(ix3, iy3, 24, 24, rgb565(150,110,60));
        fb_drawline(ix3, iy3, ix3+23, iy3+23, rgb565(100,60,30));
        fb_drawline(ix3+23, iy3, ix3, iy3+23, rgb565(100,60,30));
        draw_text(ix3 + 32, iy3 + 4, "Sokoban", COLOR_WHITE);

        /* Calculator */
        int ix4 = sm_x + 10, iy4 = sm_y + 154;
        fb_fillrect(ix4, iy4, 24, 24, rgb565(200,200,200));
        fb_fillrect(ix4+4, iy4+4, 16, 4, rgb565(100,100,100)); // screen
        fb_fillrect(ix4+4, iy4+10, 4, 4, rgb565(50,50,50)); // keys
        fb_fillrect(ix4+10, iy4+10, 4, 4, rgb565(50,50,50));
        fb_fillrect(ix4+16, iy4+10, 4, 4, rgb565(50,50,50));
        draw_text(ix4 + 32, iy4 + 4, "Calc", COLOR_WHITE);
        
        /* SysInfo */
        int ix5 = sm_x + 10, iy5 = sm_y + 194;
        fb_fillrect(ix5, iy5, 24, 24, rgb565(50,50,150));
        fb_fillrect(ix5+8, iy5+4, 8, 4, COLOR_WHITE); // 'i' dot
        fb_fillrect(ix5+8, iy5+10, 8, 10, COLOR_WHITE); // 'i' stem
        draw_text(ix5 + 32, iy5 + 4, "SysInfo", COLOR_WHITE);

        /* MemView */
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
