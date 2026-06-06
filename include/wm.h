/* ============================================================================
 * TIOS — wm.h
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
    
    /* Callback to render the client area. x, y are absolute screen coordinates */
    void (*draw_client)(struct window *win, int client_x, int client_y, int client_w, int client_h);
    
    struct window *next;
} window_t;

void wm_init(void);
window_t *wm_add_window(int x, int y, int w, int h, const char *title, void (*draw_cb)(window_t*, int, int, int, int));
void wm_render(void);
void wm_update(void); /* handles input */

#endif
