/* ============================================================================
 * STAX — wm_internal.h
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

#ifndef DOOM_WIN_MARKER
#define DOOM_WIN_MARKER ((void *)0xD00D0001u)
#endif

#define RGB565_C(r, g, b) (uint16_t)((((b) & 0xF8u) << 8) | (((g) & 0xFCu) << 3) | ((r) >> 3))

extern int bg_color_idx;
extern uint16_t bg_colors[5];

#define COL_DESKTOP     (bg_colors[bg_color_idx])
#define COL_WIN_BG      rgb565(240, 240, 245)
#define COL_WIN_TITLE   rgb565(220, 220, 225)
#define COL_WIN_TITLE_TXT COLOR_BLACK
#define COL_WIN_BORDER_LIGHT rgb565(180, 180, 185)
#define COL_WIN_BORDER_DARK  rgb565(150, 150, 155)
#define COL_TASKBAR     rgb565(192, 192, 192)

extern window_t *window_list;
extern window_t *focused_window;

extern context_menu_t ctx_menu;
extern int stax_menu_active;
extern int apps_menu_active;

#define ICON_W          64
#define ICON_H          74
#define ICON_GRID_W     86
#define ICON_GRID_H     86

typedef struct {
    int id;
    int x, y;
    const char *name;
} app_icon_t;

#define NUM_APPS 9
extern app_icon_t app_icons[NUM_APPS];

#define DESK_MAX        24
#define DESK_REFRESH_MS 3000
#define DESK_START_X    18
#define DESK_ICON_W     86
#define DESK_ICON_H     86

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
void desk_save_positions(void);
void draw_window(window_t *win);

/* ---- Ctrl+Tab Window Switcher ---- */
/* Shared between wm.c (input) and wm_render.c (drawing) */

#define SWITCHER_MAX_WINS 16  /* max windows shown in the switcher */

typedef struct {
    int     active;          /* 1 = overlay is visible */
    int     sel;             /* index into wins[] of currently selected window */
    int     count;           /* number of windows in wins[] */
    window_t *wins[SWITCHER_MAX_WINS];
    unsigned open_tick;      /* tick_count when overlay opened (for entry anim) */
    unsigned tab_tick;       /* tick_count of last Tab press (for selection anim) */
    int      anim_x_fp;      /* fixed-point animated x coordinate for smooth sliding box */
    int      anim_inited;    /* 1 if animated x is initialized */
} wm_switcher_t;

extern wm_switcher_t g_switcher;

/* Open the switcher and step in given direction (+1 next, -1 prev) */
void switcher_step(int dir);
void switcher_open_or_advance(void);
/* Commit the current selection and close the overlay */
void switcher_commit(void);
/* Cancel switcher without changing window */
void switcher_cancel(void);
/* Draw the switcher overlay — called at end of wm_render() */
void switcher_draw(void);

#endif
