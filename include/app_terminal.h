/* ============================================================================
 * STAX — app_terminal.h
 * ============================================================================ */
#ifndef APP_TERMINAL_H
#define APP_TERMINAL_H

struct window;

void terminal_draw_window(struct window *win, int cx, int cy, int cw, int ch);
void terminal_key_event(struct window *win, char c);

#endif
