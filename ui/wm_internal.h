/* ============================================================================
 * TIOS — wm_internal.h
 * Internal definitions for the Window Manager
 * ============================================================================ */
#ifndef WM_INTERNAL_H
#define WM_INTERNAL_H

#include "wm.h"
#include "framebuffer.h"
#include "mouse.h"
#include "string.h"
#include "heap.h"
#include "fatfs/ff.h"

#define TITLEBAR_HEIGHT 20
#define BORDER_WIDTH    2
#define TASKBAR_HEIGHT  28

#define RGB565_C(r, g, b) (((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3))

extern int bg_color_idx;
extern uint16_t bg_colors[5];

#define COL_DESKTOP     (bg_colors[bg_color_idx])
#define COL_WIN_BG      rgb565(192, 192, 192)
#define COL_WIN_TITLE   rgb565(0, 0, 128)
#define COL_WIN_TITLE_TXT COLOR_WHITE
#define COL_WIN_BORDER_LIGHT COLOR_WHITE
#define COL_WIN_BORDER_DARK  rgb565(128, 128, 128)
#define COL_TASKBAR     rgb565(192, 192, 192)

extern window_t *window_list;
extern window_t *focused_window;

extern context_menu_t ctx_menu;
extern int start_menu_active;

#define ICON_W       64
#define ICON_H       64
#define ICON_SPACING 80

typedef struct {
    int id;
    int x, y;
    const char *name;
} app_icon_t;

#define NUM_APPS 3
extern app_icon_t app_icons[NUM_APPS];

#define DESK_MAX        20
#define DESK_REFRESH_MS 3000
#define DESK_START_X    100
#define DESK_ICON_W     74
#define DESK_ICON_H     80

typedef struct {
    char name[16];
    int  is_dir;
    int  x, y;
    int  valid;
} desk_file_t;

extern desk_file_t desk_files[DESK_MAX];
extern int         desk_count;
extern int         desk_loaded;
extern int         desk_refresh;

extern uint16_t *desktop_bg_image;

#define CURSOR_W 11
#define CURSOR_H 16
extern const char cursor_bitmap[CURSOR_H][CURSOR_W];

void wm_bring_to_front(window_t *win);
void desk_load_files(void);
void draw_window(window_t *win);

#endif
