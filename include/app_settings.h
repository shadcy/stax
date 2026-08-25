#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include "wm.h"
#include <stdint.h>

#define SETTINGS_MAGIC 0x53545832 /* 'STX2' */

typedef struct {
    uint32_t magic;
    uint32_t version;
    int      show_boot_log_on_startup;
    int      boot_win_x;
    int      boot_win_y;
    int      boot_win_w;
    int      boot_win_h;
    int      time_format_24h;
    int      bg_color_idx;
    int      resolution_w;
    int      resolution_h;
    int      active_tab;
    int      network_enabled;    /* 1 = Internet Online, 0 = Offline/Disabled */
    int      widgets_active;     /* 1 = Desktop Widgets Active */
} sys_settings_t;

extern sys_settings_t g_settings;

void settings_init(void);
void settings_load(void);
void settings_save(void);
void settings_draw_window(struct window *win, int cx, int cy, int cw, int ch);
void settings_mouse_click(struct window *win, int mx, int my, int button);

#endif

