/* ============================================================================
 * STAX — wm.h
 * Compositing Window Manager
 * ============================================================================ */
#ifndef WM_H
#define WM_H

#include <stdint.h>

#define WM_STATE_HIDDEN    0
#define WM_STATE_MINIMIZED 1
#define WM_STATE_ACTIVE    2

typedef struct window {
    int id;
    int x, y;
    int width, height;
    char title[32];
    int state;
    
    int is_maximized;
    int saved_x, saved_y;
    int saved_width, saved_height;
    
    char path[64]; /* Custom path or data string for the app */
    void *app_data; /* Custom state pointer for the app */
    
    /* Callback to render the client area. x, y are absolute screen coordinates */
    void (*draw_client)(struct window *win, int client_x, int client_y, int client_w, int client_h);
    
    /* Callback to update per-frame logic (optional) */
    void (*update_client)(struct window *win, int dt_ms);
    
    /* Callback for keyboard input to the focused window (optional) */
    void (*key_event)(struct window *win, char c);
    
    /* Callback for mouse click inside the client area (optional). mx/my are relative to client_x/client_y */
    void (*mouse_click)(struct window *win, int mx, int my, int button);
    
    /* Callback for mouse drag inside the client area (optional). */
    void (*mouse_drag)(struct window *win, int mx, int my);
    
    struct window *next;
} window_t;

typedef struct {
    int active;
    int x, y;
} context_menu_t;

void wm_init(void);
void wm_load_background(const char *filename);
window_t *wm_add_window(int x, int y, int w, int h, const char *title, void (*draw_cb)(window_t*, int, int, int, int));
void wm_close_window(window_t *win);
void wm_focus_shell(void);
void wm_render(void);
void wm_update(void); /* handles input */
int wm_dispatch_key(char c);
void draw_text(int x, int y, const char *s, uint16_t color);

#endif
