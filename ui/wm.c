/* ============================================================================
 * STAX — wm.c
 * Compositing Window Manager Core
 * ============================================================================ */

#include "wm_internal.h"
#include "app_file_manager.h"
#include "app_terminal.h"
#include "app_editor.h"
#include "doom.h"
#include "gfx_console.h"

window_t *window_list = NULL;
static int next_id = 1;

static window_t *drag_win = NULL;
static int drag_off_x = 0;
static int drag_off_y = 0;
static int prev_mouse_b = 0;
window_t *focused_window = NULL;
static window_t *drag_client_win = NULL;

context_menu_t ctx_menu = {0, 0, 0};
int start_menu_active = 0;

static int drag_type = -1; /* 0 = app, 1 = file, -1 = none */
static int drag_idx = -1;
static int drag_moved = 0;

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
    win->mouse_drag = NULL;
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
        if (ctx_menu.active) {
            if (pressed && mx >= ctx_menu.x && mx < ctx_menu.x + 160 && 
                my >= ctx_menu.y && my < ctx_menu.y + 180) {
                
                int item = (my - ctx_menu.y) / 30;
                if (item == 0) {
                    extern struct window *terminal_open_new(void);
                    terminal_open_new();
                } else if (item == 1) {
                    extern void cmd_browser(int, char**);
                    cmd_browser(0, 0);
                } else if (item == 2) {
                    window_t *fw = wm_add_window(120, 100, 440, 330, "File Manager", file_manager_draw_window);
                    if (fw) { fw->mouse_click = file_manager_click; fw->update_client = file_manager_update; }
                } else if (item == 3) {
                    extern void editor_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                    extern void editor_key_event(struct window *win, char c);
                    window_t *ew = wm_add_window(140, 90, 500, 350, "Untitled.txt", editor_draw_window);
                    if (ew) ew->key_event = editor_key_event;
                } else if (item == 4) {
                    extern void calculator_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                    extern void calculator_mouse_click(struct window *win, int mx, int my, int button);
                    extern void calculator_key_event(struct window *win, char c);
                    window_t *cw = wm_add_window(160, 80, 280, 350, "Calculator", calculator_draw_window);
                    if (cw) {
                        cw->mouse_click = calculator_mouse_click;
                        cw->key_event = calculator_key_event;
                    }
                } else if (item == 5) {
                    extern void settings_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                    extern void settings_mouse_click(struct window *win, int mx, int my, int button);
                    window_t *sw = wm_add_window(130, 48, 580, 370, "Settings", settings_draw_window);
                    if (sw) sw->mouse_click = settings_mouse_click;
                } else if (item == 6) {
                    extern void settings_save(void);
                    extern void system_reboot(void);
                    settings_save();
                    system_reboot();
                }
            }
            ctx_menu.active = 0;
            if (!right_pressed) goto update_done;
        }        if (my < TASKBAR_HEIGHT) {
            if (pressed) {
                if (mx >= 0 && mx < 60) {
                    start_menu_active = !start_menu_active;
                } else {
                    start_menu_active = 0;
                }
            }
            drag_win = NULL;
        } else {
            if (pressed && start_menu_active) {
                int sm_x = 0;
                int sm_y = TASKBAR_HEIGHT;
                int sm_w = 200;
                int sm_h = 240;
                if (mx >= sm_x && mx < sm_x + sm_w && my >= sm_y && my < sm_y + sm_h) {
                    int rel_y = my - sm_y;
                    if (rel_y >= 0 && rel_y < 30) {
                        extern void sysinfo_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                        wm_add_window(110, 80, 340, 260, "System Info", sysinfo_draw_window);
                        start_menu_active = 0;
                    } else if (rel_y >= 30 && rel_y < 60) {
                        extern void cmd_browser(int, char**);
                        cmd_browser(0, 0);
                        start_menu_active = 0;
                    } else if (rel_y >= 60 && rel_y < 90) {
                        extern struct window *terminal_open_new(void);
                        terminal_open_new();
                        start_menu_active = 0;
                    } else if (rel_y >= 90 && rel_y < 120) {
                        window_t *fw = wm_add_window(120, 100, 440, 330, "File Manager", file_manager_draw_window);
                        if (fw) { fw->mouse_click = file_manager_click; fw->update_client = file_manager_update; }
                        start_menu_active = 0;
                    } else if (rel_y >= 120 && rel_y < 150) {
                        extern void taskmgr_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                        wm_add_window(130, 90, 420, 300, "Task Manager", taskmgr_draw_window);
                        start_menu_active = 0;
                    } else if (rel_y >= 150 && rel_y < 180) {
                        extern void settings_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                        extern void settings_mouse_click(struct window *win, int mx, int my, int button);
                        window_t *sw = wm_add_window(130, 48, 580, 370, "Settings", settings_draw_window);
                        if (sw) sw->mouse_click = settings_mouse_click;
                        start_menu_active = 0;
                    } else if (rel_y >= 180 && rel_y < 210) {
                        extern void settings_save(void);
                        extern void system_reboot(void);
                        settings_save();
                        system_reboot();
                    } else if (rel_y >= 210 && rel_y <= 240) {
                        /* Force Quit */
                        extern void wm_close_window(window_t *win);
                        if (focused_window) {
                            wm_close_window(focused_window);
                        } else if (window_list) {
                            wm_close_window(window_list);
                        }
                        extern volatile int stax_doom_quit_requested;
                        stax_doom_quit_requested = 1;
                        start_menu_active = 0;
                    } else {
                        start_menu_active = 0;
                    }
                } else {
                    start_menu_active = 0;
                }
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
                        int btn_w = 12;
                        int close_x = curr->x + BORDER_WIDTH + 8;
                        int min_x   = close_x + btn_w + 6;
                        int max_x   = min_x + btn_w + 6;
                        
                        if (my >= tby + 4 && my < tby + 4 + btn_w) {
                            if (mx >= close_x && mx < close_x + btn_w) {
                                if (curr->app_data == DOOM_WIN_MARKER) {
                                    doom_force_cleanup();
                                    if (focused_window == curr)
                                        focused_window = NULL;
                                    break;
                                }
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
                                break;
                            }
                            if (mx >= max_x && mx < max_x + btn_w) {
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
                                    curr->y = TASKBAR_HEIGHT;
                                    curr->width = fb_width;
                                    curr->height = fb_height - TASKBAR_HEIGHT;
                                    curr->is_maximized = 1;
                                }
                                break;
                            }
                            if (mx >= min_x && mx < min_x + btn_w) {
                                curr->state = WM_STATE_MINIMIZED;
                                if (focused_window == curr) focused_window = NULL;
                                break;
                            }
                        }
                        
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
                                    curr->y = TASKBAR_HEIGHT;
                                    curr->width = fb_width;
                                    curr->height = fb_height - TASKBAR_HEIGHT;
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
                            drag_client_win = curr;
                            if (curr->mouse_click) {
                                curr->mouse_click(curr, mx - curr->x, my - curr->y - BORDER_WIDTH - TITLEBAR_HEIGHT, mb & 3);
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
                    if (ctx_menu.x + 150 > (int)fb_width) ctx_menu.x = fb_width - 150;
                    if (ctx_menu.y + 150 > (int)(fb_height - TASKBAR_HEIGHT)) ctx_menu.y = fb_height - TASKBAR_HEIGHT - 150;
                } else if (pressed) {
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
        drag_client_win = NULL;
        if (drag_type != -1 && !drag_moved) {
            if (drag_type == 0 && drag_idx >= 0 && drag_idx < NUM_APPS) {
                int i = drag_idx;
                if (app_icons[i].id == 0) {
                    extern void cmd_browser(int, char**);
                    cmd_browser(0, 0);
                } else if (app_icons[i].id == 1) {
                    extern struct window *terminal_open_new(void);
                    terminal_open_new();
                } else if (app_icons[i].id == 2) {
                    window_t *fw = wm_add_window(120, 100, 440, 330, "File Manager", file_manager_draw_window);
                    if (fw) { fw->mouse_click = file_manager_click; fw->update_client = file_manager_update; }
                } else if (app_icons[i].id == 3) {
                    extern void editor_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                    extern void editor_key_event(struct window *win, char c);
                    window_t *ew = wm_add_window(140, 90, 500, 350, "Untitled.txt", editor_draw_window);
                    if (ew) ew->key_event = editor_key_event;
                } else if (app_icons[i].id == 4) {
                    extern void calculator_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                    extern void calculator_mouse_click(struct window *win, int mx, int my, int button);
                    extern void calculator_key_event(struct window *win, char c);
                    window_t *cw = wm_add_window(160, 80, 280, 350, "Calculator", calculator_draw_window);
                    if (cw) {
                        cw->mouse_click = calculator_mouse_click;
                        cw->key_event = calculator_key_event;
                    }
                } else if (app_icons[i].id == 5) {
                    extern void sysinfo_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                    wm_add_window(110, 80, 340, 260, "System Info", sysinfo_draw_window);
                } else if (app_icons[i].id == 6) {
                    extern void taskmgr_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                    wm_add_window(130, 90, 420, 300, "Task Manager", taskmgr_draw_window);
                } else if (app_icons[i].id == 7) {
                    extern void cmd_doomgfx(int, char**);
                    cmd_doomgfx(0, 0);
                } else if (app_icons[i].id == 8) {
                    extern void settings_draw_window(struct window *win, int cx, int cy, int cw, int ch);
                    extern void settings_mouse_click(struct window *win, int mx, int my, int button);
                    window_t *sw = wm_add_window(130, 48, 580, 370, "Settings", settings_draw_window);
                    if (sw) sw->mouse_click = settings_mouse_click;
                }
            } else if (drag_type == 1 && drag_idx >= 0 && drag_idx < desk_count) {
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
            if (drag_type == 0 && drag_idx >= 0 && drag_idx < NUM_APPS) {
                int col = (app_icons[drag_idx].x - 18 + ICON_GRID_W/2) / ICON_GRID_W;
                if (col < 0) col = 0;
                if (col > 1) col = 1;
                int row = (app_icons[drag_idx].y - 42 + ICON_GRID_H/2) / ICON_GRID_H;
                if (row < 0) row = 0;
                app_icons[drag_idx].x = (col == 0) ? 18 : 104;
                app_icons[drag_idx].y = 42 + row * 86;
            } else if (drag_type == 1 && drag_idx >= 0 && drag_idx < desk_count) {
                int col = (desk_files[drag_idx].x - DESK_START_X + DESK_ICON_W/2) / DESK_ICON_W;
                if (col < 0) col = 0;
                int row = (desk_files[drag_idx].y - 42 + DESK_ICON_H/2) / DESK_ICON_H;
                if (row < 0) row = 0;
                desk_files[drag_idx].x = DESK_START_X + col * DESK_ICON_W;
                desk_files[drag_idx].y = 42 + row * DESK_ICON_H;
            }
        }
        drag_type = -1;
    }
    
    if (mb & 1) {
        if (drag_win) {
            drag_win->x = mx - drag_off_x;
            drag_win->y = my - drag_off_y;
        } else if (drag_client_win && drag_client_win->mouse_drag) {
            drag_client_win->mouse_drag(drag_client_win, mx - drag_client_win->x, my - drag_client_win->y - BORDER_WIDTH - TITLEBAR_HEIGHT);
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
