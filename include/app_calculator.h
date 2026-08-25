#ifndef APP_CALCULATOR_H
#define APP_CALCULATOR_H

#include "wm.h"

void calculator_draw_window(struct window *win, int cx, int cy, int cw, int ch);
void calculator_mouse_click(struct window *win, int mx, int my, int button);
void calculator_key_event(struct window *win, char c);

#endif
