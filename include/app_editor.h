/* ============================================================================
 * TIOS — app_editor.h
 * Nano-GUI Text Editor Application
 * ============================================================================ */

#ifndef APP_EDITOR_H
#define APP_EDITOR_H

struct window;

void editor_draw_window(struct window *win, int cx, int cy, int cw, int ch);
void editor_key_event(struct window *win, char c);
void editor_autosave(struct window *win); /* called on close */

#endif
