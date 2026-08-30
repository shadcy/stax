/* ============================================================================
 * STAX — wm.c
 * Compositing Window Manager Core
 * ============================================================================ */

#include "wm_internal.h"
#include "app_file_manager.h"
#include "app_terminal.h"
#include "app_editor.h"
#include "gfx_console.h"
#include "keyboard.h"

/* Marker used to identify .stapp/DOOM windows in the WM (previously from doom.h) */
#ifndef DOOM_WIN_MARKER
#define DOOM_WIN_MARKER ((void *)0xD00D0001u)
#endif

window_t *window_list = NULL;
static int next_id = 1;

static window_t *drag_win = NULL;
static int drag_off_x = 0;
static int drag_off_y = 0;
static int prev_mouse_b = 0;
window_t *focused_window = NULL;
static window_t *drag_client_win = NULL;

context_menu_t ctx_menu = {0, 0, 0};
int stax_menu_active = 0;
int apps_menu_active = 0;

static int drag_type = -1; /* 0 = app, 1 = file, -1 = none */
static int drag_idx = -1;
static int drag_moved = 0;

void wm_init(void) {
    fb_set_double_buffering(1);
}

window_t *wm_add_window(int x, int y, int w, int h, const char *title, void (*draw_cb)(window_t*, int, int, int, int)) {
    window_t *win = (window_t*)kmalloc(sizeof(window_t));
    if (!win) return NULL;
    
    memset(win, 0, sizeof(window_t));
    
    win->id = next_id++;
    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    
    win->saved_x = x;
    win->saved_y = y;
    win->saved_width = (w > 0) ? w : 400;
    win->saved_height = (h > 0) ? h : 300;
    win->is_maximized = 0;
    
    int i;
    for (i = 0; i < 31 && title && title[i] != '\0'; i++) {
        win->title[i] = title[i];
    }
    win->title[i] = '\0';
    
    win->state = WM_STATE_ACTIVE;
    win->draw_client = draw_cb;
    
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

void wm_toggle_boot_log(void)
{
    window_t *curr = window_list;
    while (curr) {
        if (strcmp(curr->title, "Boot Log") == 0) {
            if (curr->state == WM_STATE_HIDDEN || curr->state == WM_STATE_MINIMIZED) {
                curr->state = WM_STATE_ACTIVE;
                wm_bring_to_front(curr);
                focused_window = curr;
            } else {
                curr->state = WM_STATE_HIDDEN;
                if (focused_window == curr) focused_window = NULL;
            }
            return;
        }
        curr = curr->next;
    }
    extern void gfx_console_draw_window(struct window*, int, int, int, int);
    extern void gfx_console_key_event(struct window*, char);
    extern void gfx_console_mouse_click(struct window*, int, int, int);
    extern void gfx_console_mouse_drag(struct window*, int, int);
    window_t *gw = wm_add_window(200, 44, 560, 380, "Boot Log", gfx_console_draw_window);
    if (gw) {
        gw->key_event = gfx_console_key_event;
        gw->mouse_click = gfx_console_mouse_click;
        gw->mouse_drag = gfx_console_mouse_drag;
        focused_window = gw;
    }
}

int wm_dispatch_key(char c) {
    /* 1. Global Window & System Management Shortcuts (Strictly require CTRL held) */
    if (kb_is_pressed(KB_CTRL)) {
        if (c == 't' || c == 'T' || c == 0x14) { /* Ctrl+T: New Terminal */
            extern struct window *terminal_open_new(void);
            terminal_open_new();
            return 1;
        }
        if (c == 'n' || c == 'N' || c == 0x0E) { /* Ctrl+N: New Document */
            extern void editor_draw_window(struct window *win, int cx, int cy, int cw, int ch);
            extern void editor_key_event(struct window *win, char c);
            window_t *ew = wm_add_window(140, 90, 500, 350, "Untitled.txt", editor_draw_window);
            if (ew) ew->key_event = editor_key_event;
            return 1;
        }
        if (c == 'w' || c == 'W' || c == 0x17) { /* Ctrl+W: Close active window */
            if (focused_window && focused_window->state == WM_STATE_ACTIVE) {
                if (focused_window->app_data == DOOM_WIN_MARKER) {
                    focused_window->state = WM_STATE_HIDDEN;
                    focused_window = NULL;
                    return 1;
                }
                extern void editor_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                if (focused_window->draw_client == editor_draw_window) {
                    editor_autosave(focused_window);
                    file_manager_refresh();
                }
                focused_window->state = WM_STATE_HIDDEN;
                focused_window = NULL;
                return 1;
            }
        }
        if (c == 'f' || c == 'F' || c == 0x06) { /* Ctrl+F: Maximize / Restore Toggle */
            if (focused_window && focused_window->state == WM_STATE_ACTIVE) {
                if (focused_window->is_maximized) {
                    focused_window->x = (focused_window->saved_x >= 0 && focused_window->saved_x < (int)fb_width - 50) ? focused_window->saved_x : 50;
                    focused_window->y = (focused_window->saved_y >= TASKBAR_HEIGHT && focused_window->saved_y < (int)fb_height - 50) ? focused_window->saved_y : TASKBAR_HEIGHT + 20;
                    focused_window->width = (focused_window->saved_width > 100 && focused_window->saved_width <= (int)fb_width) ? focused_window->saved_width : 500;
                    focused_window->height = (focused_window->saved_height > 100 && focused_window->saved_height <= (int)fb_height) ? focused_window->saved_height : 350;
                    focused_window->is_maximized = 0;
                } else {
                    focused_window->saved_x = focused_window->x;
                    focused_window->saved_y = focused_window->y;
                    focused_window->saved_width = focused_window->width;
                    focused_window->saved_height = focused_window->height;
                    focused_window->x = 0;
                    focused_window->y = TASKBAR_HEIGHT;
                    focused_window->width = fb_width;
                    focused_window->height = fb_height - TASKBAR_HEIGHT;
                    focused_window->is_maximized = 1;
                }
                return 1;
            }
        }
        if (c == 0x0D || c == '\n') { /* Ctrl+Enter: Minimize active window */
            if (focused_window && focused_window->state == WM_STATE_ACTIVE) {
                focused_window->state = WM_STATE_MINIMIZED;
                focused_window = NULL;
                return 1;
            }
        }
    }

    if (c == '\t' && (kb_is_pressed(KB_ALT) || kb_is_pressed(KB_CTRL))) { /* Alt+Tab / Ctrl+Tab: Cycle windows */
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

    /* Forward all regular keys and all arrow keys directly to the focused window */
    if (focused_window
        && focused_window->state == WM_STATE_ACTIVE
        && focused_window->key_event) {
        focused_window->key_event(focused_window, c);
        
        if (strcmp(focused_window->title, "Boot Log") == 0) {
            if (c == 0x11 || c == 0x12) return 1; /* Consumed for scrolling */
            return 0; /* Let typing fall through to kernel shell */
        }
        
        return 1;
    }
    return 0;
}

void wm_bring_to_front(window_t *win) {
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
        desk_refresh += dt_ms;
        if (desk_refresh >= DESK_REFRESH_MS) {
            desk_loaded = 0;
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
        if (ctx_menu.active) {
            int cm_w = 160;
            int cm_h = 112;
            if (pressed && mx >= ctx_menu.x && mx < ctx_menu.x + cm_w && 
                my >= ctx_menu.y && my < ctx_menu.y + cm_h) {
                
                int item = (my - ctx_menu.y) / 28;
                if (item == 0) {
                    extern struct window *terminal_open_new(void);
                    terminal_open_new();
                } else if (item == 1) {
                    extern void editor_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                    extern void editor_key_event(struct window *win, char c);
                    window_t *ew = wm_add_window(140, 90, 500, 350, "Untitled.txt", editor_draw_window);
                    if (ew) ew->key_event = editor_key_event;
                } else if (item == 2) {
                    desk_load_files();
                } else if (item == 3) {
                    extern void settings_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                    extern void settings_mouse_click(struct window *win, int mx, int my, int button);
                    window_t *sw = wm_add_window(130, 48, 580, 370, "Settings", settings_draw_window);
                    if (sw) sw->mouse_click = settings_mouse_click;
                }
            }
            ctx_menu.active = 0;
            if (!right_pressed) goto update_done;
        }        if (my < TASKBAR_HEIGHT) {
            if (pressed) {
                if (mx >= 0 && mx < 44) {
                    stax_menu_active = !stax_menu_active;
                    apps_menu_active = 0;
                } else if (mx >= 46 && mx < 124) {
                    apps_menu_active = !apps_menu_active;
                    stax_menu_active = 0;
                } else {
                    stax_menu_active = 0;
                    apps_menu_active = 0;
                    /* Check window tab clicks */
                    int nav_x = 128;
                    int max_nav_x = (int)fb_width - 280;
                    int avail_w = max_nav_x - nav_x;
                    extern struct window *window_list;
                    window_t *arr[32];
                    int count = 0;
                    window_t *curr = window_list;
                    while (curr && count < 32) {
                        if (curr->state != WM_STATE_HIDDEN) arr[count++] = curr;
                        curr = curr->next;
                    }
                    if (count > 0 && avail_w > 80) {
                        int tab_gap = 4;
                        int tab_w = (avail_w - (count - 1) * tab_gap) / count;
                        if (tab_w > 130) tab_w = 130;
                        int visible_tabs = count;
                        if (tab_w < 55) {
                            tab_w = 55;
                            int max_fit = (avail_w - 45) / (tab_w + tab_gap);
                            if (max_fit < 1) max_fit = 1;
                            visible_tabs = max_fit;
                        }
                        for (int i = 0; i < visible_tabs; i++) {
                            int tx = nav_x + i * (tab_w + tab_gap);
                            if (mx >= tx && mx < tx + tab_w) {
                                window_t *w = arr[i];
                                if (w->state == WM_STATE_MINIMIZED) {
                                    w->state = WM_STATE_ACTIVE;
                                    wm_bring_to_front(w);
                                } else if (w == arr[0] && w->state == WM_STATE_ACTIVE) {
                                    w->state = WM_STATE_MINIMIZED;
                                } else {
                                    wm_bring_to_front(w);
                                }
                                break;
                            }
                        }
                    }
                }
            }
            drag_win = NULL;
        } else {
            /* 1. STAX System Dropdown Menu Clicks */
            if (pressed && stax_menu_active) {
                int sm_x = 0;
                int sm_y = TASKBAR_HEIGHT;
                int sm_w = 190;
                int sm_h = 195;
                if (mx >= sm_x && mx < sm_x + sm_w && my >= sm_y && my < sm_y + sm_h) {
                    int rel_y = my - sm_y;
                    if (rel_y >= 0 && rel_y < 30) {
                        extern void sysinfo_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                        wm_add_window(110, 80, 340, 260, "About STAX OS", sysinfo_draw_window);
                    } else if (rel_y >= 30 && rel_y < 60) {
                        extern void settings_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                        extern void settings_mouse_click(struct window *win, int mx, int my, int button);
                        window_t *sw = wm_add_window(130, 48, 580, 370, "Settings", settings_draw_window);
                        if (sw) sw->mouse_click = settings_mouse_click;
                    } else if (rel_y >= 60 && rel_y < 90) {
                        extern void taskmgr_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                        wm_add_window(130, 90, 420, 300, "Task Manager", taskmgr_draw_window);
                    } else if (rel_y >= 90 && rel_y < 120) {
                        extern void sysinfo_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                        wm_add_window(110, 80, 340, 260, "System Info", sysinfo_draw_window);
                    } else if (rel_y >= 120 && rel_y < 155) {
                        extern void settings_save(void);
                        extern void system_reboot(void);
                        settings_save();
                        system_reboot();
                    } else if (rel_y >= 185 && rel_y <= 225) {
                        extern void wm_close_window(window_t *win);
                        if (focused_window) {
                            wm_close_window(focused_window);
                        } else if (window_list) {
                            wm_close_window(window_list);
                        }
                    }
                }
                stax_menu_active = 0;
            }

            /* 2. Apps Launcher Menu Clicks */
            if (pressed && apps_menu_active) {
                int app_x = 50;
                int app_y = TASKBAR_HEIGHT + 2;
                int app_w = 340;
                int app_h = 390;
                if (mx >= app_x && mx < app_x + app_w && my >= app_y && my < app_y + app_h) {
                    int rel_x = mx - app_x;
                    int rel_y = my - app_y;

                    if (rel_y >= 38 && rel_y < 340) {
                        int col = (rel_x - 12) / 106;
                        int row = (rel_y - 38) / 98;
                        if (col >= 0 && col < 3 && row >= 0 && row < 3) {
                            int aid = row * 3 + col;
                            if (aid == 0) {
                                extern void cmd_browser(int, char**);
                                cmd_browser(0, 0);
                            } else if (aid == 1) {
                                extern struct window *terminal_open_new(void);
                                terminal_open_new();
                            } else if (aid == 2) {
                                window_t *fw = wm_add_window(120, 100, 440, 330, "File Manager", file_manager_draw_window);
                                if (fw) { fw->mouse_click = file_manager_click; fw->update_client = file_manager_update; }
                            } else if (aid == 3) {
                                extern void editor_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                                extern void editor_key_event(struct window *win, char c);
                                window_t *ew = wm_add_window(140, 90, 500, 350, "Untitled.txt", editor_draw_window);
                                if (ew) ew->key_event = editor_key_event;
                            } else if (aid == 4) {
                                extern void calculator_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                                extern void calculator_mouse_click(struct window *win, int mx, int my, int button);
                                extern void calculator_key_event(struct window *win, char c);
                                window_t *cw = wm_add_window(160, 80, 280, 350, "Calculator", calculator_draw_window);
                                if (cw) {
                                    cw->mouse_click = calculator_mouse_click;
                                    cw->key_event = calculator_key_event;
                                }
                            } else if (aid == 5) {
                                extern void sysinfo_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                                wm_add_window(110, 80, 340, 260, "System Info", sysinfo_draw_window);
                            } else if (aid == 6) {
                                extern void taskmgr_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                                wm_add_window(130, 90, 420, 300, "Task Manager", taskmgr_draw_window);
                            } else if (aid == 7) {
                                extern void cmd_doomgfx(int, char**);
                                cmd_doomgfx(0, 0);
                            } else if (aid == 8) {
                                extern void settings_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                                extern void settings_mouse_click(struct window *win, int mx, int my, int button);
                                window_t *sw = wm_add_window(130, 48, 580, 370, "Settings", settings_draw_window);
                                if (sw) sw->mouse_click = settings_mouse_click;
                            }
                            apps_menu_active = 0;
                        }
                    } else if (rel_y >= 348 && rel_y <= 385) {
                        if (rel_x >= 12 && rel_x < 110) {
                            extern void sysinfo_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                            wm_add_window(110, 80, 340, 260, "System Info", sysinfo_draw_window);
                        } else if (rel_x >= 118 && rel_x < 222) {
                            extern void settings_save(void);
                            extern void system_reboot(void);
                            settings_save();
                            system_reboot();
                        } else if (rel_x >= 230 && rel_x < 328) {
                            extern void wm_close_window(window_t *win);
                            if (focused_window) {
                                wm_close_window(focused_window);
                            } else if (window_list) {
                                wm_close_window(window_list);
                            }
                        }
                    }
                }
                apps_menu_active = 0;
            }
            
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
                        
                        int tby = curr->y + BORDER_WIDTH;
                        int tbw = curr->width - BORDER_WIDTH * 2;
                        int tbx = curr->x + BORDER_WIDTH;
                        int close_x = tbx + tbw - 20;
                        int max_x   = close_x - 18;
                        int min_x   = max_x - 18;
                        int btn_w   = 16;
                        int btn_handled = 0;
                        
                        if (my >= tby + 2 && my < tby + TITLEBAR_HEIGHT - 2) {
                            if (mx >= close_x - 2 && mx < close_x + btn_w) {
                                btn_handled = 1;
                                if (curr->app_data == DOOM_WIN_MARKER) {
                                    curr->state = WM_STATE_HIDDEN;
                                    if (focused_window == curr)
                                        focused_window = NULL;
                                } else {
                                    extern void editor_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                                    if (curr->draw_client == editor_draw_window) {
                                        editor_autosave(curr);
                                        file_manager_refresh();
                                    }
                                    curr->state = WM_STATE_HIDDEN;
                                    extern void image_viewer_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                                    if (curr->draw_client == image_viewer_draw_window && curr->app_data && curr->app_data != (void*)1) {
                                        extern void kfree(void*);
                                        kfree(curr->app_data);
                                        curr->app_data = NULL;
                                    }
                                    if (focused_window == curr) focused_window = NULL;
                                }
                            } else if (mx >= max_x - 2 && mx < max_x + btn_w) {
                                btn_handled = 1;
                                if (curr->is_maximized) {
                                    curr->x = (curr->saved_x >= 0 && curr->saved_x < (int)fb_width - 50) ? curr->saved_x : 50;
                                    curr->y = (curr->saved_y >= TASKBAR_HEIGHT && curr->saved_y < (int)fb_height - 50) ? curr->saved_y : TASKBAR_HEIGHT + 20;
                                    curr->width = (curr->saved_width > 100 && curr->saved_width <= (int)fb_width) ? curr->saved_width : 500;
                                    curr->height = (curr->saved_height > 100 && curr->saved_height <= (int)fb_height) ? curr->saved_height : 350;
                                    curr->is_maximized = 0;
                                } else {
                                    curr->saved_x = curr->x;
                                    curr->saved_y = curr->y;
                                    curr->saved_width = curr->width;
                                    curr->saved_height = curr->height;
                                    curr->x = 0;
                                    curr->y = TASKBAR_HEIGHT;
                                    curr->width = fb_width;
                                    curr->height = fb_height - TASKBAR_HEIGHT;
                                    curr->is_maximized = 1;
                                }
                            } else if (mx >= min_x - 2 && mx < min_x + btn_w) {
                                btn_handled = 1;
                                curr->state = WM_STATE_MINIMIZED;
                                if (focused_window == curr) focused_window = NULL;
                            }
                        }
                        
                        if (!btn_handled) {
                            if (my >= curr->y && my < curr->y + BORDER_WIDTH + TITLEBAR_HEIGHT) {
                                extern volatile unsigned int tick_count;
                                static unsigned int last_click = 0;
                                static window_t *last_win = NULL;
                                if (last_win == curr && (tick_count - last_click) < 300) {
                                    if (curr->is_maximized) {
                                        curr->x = (curr->saved_x >= 0 && curr->saved_x < (int)fb_width - 50) ? curr->saved_x : 50;
                                        curr->y = (curr->saved_y >= TASKBAR_HEIGHT && curr->saved_y < (int)fb_height - 50) ? curr->saved_y : TASKBAR_HEIGHT + 20;
                                        curr->width = (curr->saved_width > 100 && curr->saved_width <= (int)fb_width) ? curr->saved_width : 500;
                                        curr->height = (curr->saved_height > 100 && curr->saved_height <= (int)fb_height) ? curr->saved_height : 350;
                                        curr->is_maximized = 0;
                                    } else {
                                        curr->saved_x = curr->x;
                                        curr->saved_y = curr->y;
                                        curr->saved_width = curr->width;
                                        curr->saved_height = curr->height;
                                        curr->x = 0;
                                        curr->y = TASKBAR_HEIGHT;
                                        curr->width = fb_width;
                                        curr->height = fb_height - TASKBAR_HEIGHT;
                                        curr->is_maximized = 1;
                                    }
                                    last_click = 0;
                                } else {
                                    if (!curr->is_maximized) {
                                        drag_win = curr;
                                        drag_off_x = mx - curr->x;
                                        drag_off_y = my - curr->y;
                                    }
                                    last_click = tick_count;
                                    last_win = curr;
                                }
                            } else {
                                drag_client_win = curr;
                                if (curr->mouse_click) {
                                    curr->mouse_click(curr, mx - curr->x, my - curr->y - BORDER_WIDTH - TITLEBAR_HEIGHT, mb & 3);
                                }
                            }
                        }
                    } else if (right_pressed) {
                        focused_window = curr;
                        wm_bring_to_front(curr);
                        if (curr->mouse_click && my >= curr->y + BORDER_WIDTH + TITLEBAR_HEIGHT) {
                            curr->mouse_click(curr, mx - curr->x, my - curr->y - BORDER_WIDTH - TITLEBAR_HEIGHT, mb & 3);
                        }
                    }
                    break;
                }
                curr = curr->next;
            }
            
            if (!hit_window) {
                if (pressed || right_pressed) focused_window = NULL;
                if (right_pressed) {
                    ctx_menu.active = 1;
                    ctx_menu.x = mx;
                    ctx_menu.y = my;
                    if (ctx_menu.x + 160 > (int)fb_width) ctx_menu.x = fb_width - 160;
                    if (ctx_menu.y + 112 > (int)(fb_height - TASKBAR_HEIGHT)) ctx_menu.y = fb_height - TASKBAR_HEIGHT - 112;
                } else if (pressed) {
                    if (!desk_loaded) desk_load_files();
                    for (int i = 0; i < desk_count; i++) {
                        if (!desk_files[i].valid) continue;
                        int ix = desk_files[i].x;
                        int iy = desk_files[i].y;
                        if (mx >= ix && mx < ix + DESK_ICON_W && my >= iy && my < iy + DESK_ICON_H) {
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
        drag_client_win = NULL;
        if (drag_type == 1 && !drag_moved && drag_idx >= 0 && drag_idx < desk_count) {
            int i = drag_idx;
            if (desk_files[i].is_dir) {
                window_t *fw=wm_add_window(60,60,420,320,desk_files[i].name,file_manager_draw_window);
                if (fw) {
                    fw->mouse_click=file_manager_click;
                    fw->update_client=file_manager_update;
                    int k=0; while(desk_files[i].name[k]) { fw->path[k]=desk_files[i].name[k]; k++; }
                    fw->path[k]='\0';
                }
            } else {
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
                } else if ((nlen > 7 &&
                    (desk_files[i].name[nlen-7]=='.' || desk_files[i].name[nlen-7]=='.') &&
                    (desk_files[i].name[nlen-6]=='L'||desk_files[i].name[nlen-6]=='l') &&
                    (desk_files[i].name[nlen-5]=='A'||desk_files[i].name[nlen-5]=='a') &&
                    (desk_files[i].name[nlen-4]=='U'||desk_files[i].name[nlen-4]=='u') &&
                    (desk_files[i].name[nlen-3]=='N'||desk_files[i].name[nlen-3]=='n') &&
                    (desk_files[i].name[nlen-2]=='C'||desk_files[i].name[nlen-2]=='c') &&
                    (desk_files[i].name[nlen-1]=='H'||desk_files[i].name[nlen-1]=='h')) ||
                    (nlen > 5 &&
                    (desk_files[i].name[nlen-5]=='S'||desk_files[i].name[nlen-5]=='s') &&
                    (desk_files[i].name[nlen-4]=='T'||desk_files[i].name[nlen-4]=='t') &&
                    (desk_files[i].name[nlen-3]=='A'||desk_files[i].name[nlen-3]=='a') &&
                    (desk_files[i].name[nlen-2]=='P'||desk_files[i].name[nlen-2]=='p') &&
                    (desk_files[i].name[nlen-1]=='P'||desk_files[i].name[nlen-1]=='p'))) {

                    /* .launch / .stapp — launch as application package */
                    extern int launch_exec(const char *path);
                    char launch_path[64];
                    launch_path[0] = '/';
                    int k = 0;
                    while (desk_files[i].name[k] && k < 60) {
                        launch_path[k+1] = desk_files[i].name[k]; k++;
                    }
                    launch_path[k+1] = '\0';
                    launch_exec(launch_path);
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
        } else if (drag_type == 1 && drag_moved && drag_idx >= 0 && drag_idx < desk_count) {
            int col = (desk_files[drag_idx].x - DESK_START_X + DESK_ICON_W/2) / DESK_ICON_W;
            if (col < 0) col = 0;
            int row = (desk_files[drag_idx].y - 42 + DESK_ICON_H/2) / DESK_ICON_H;
            if (row < 0) row = 0;
            desk_files[drag_idx].x = DESK_START_X + col * DESK_ICON_W;
            desk_files[drag_idx].y = 42 + row * DESK_ICON_H;
            desk_save_positions();
        }
        drag_type = -1;
    }
    
    if (mb & 1) {
        if (drag_win) {
            drag_win->x = mx - drag_off_x;
            drag_win->y = my - drag_off_y;
        } else if (drag_client_win && drag_client_win->mouse_drag) {
            drag_client_win->mouse_drag(drag_client_win, mx - drag_client_win->x, my - drag_client_win->y - BORDER_WIDTH - TITLEBAR_HEIGHT);
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
