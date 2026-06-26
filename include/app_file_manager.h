/* ============================================================================
 * STAX — app_file_manager.h
 * ============================================================================ */
#ifndef APP_FILE_MANAGER_H
#define APP_FILE_MANAGER_H

struct window;

void file_manager_draw_window(struct window *win, int cx, int cy, int cw, int ch);
void file_manager_update(struct window *win, int dt_ms);   /* real-time auto-refresh */
void file_manager_click(struct window *win, int mx, int my, int button);
void file_manager_key_rename(struct window *win, char c);
void file_manager_refresh(void);

#endif
