/* ============================================================================
 * STAX — app_widgets.h
 * Retro Internet-Powered Desktop & Windowed Widgets (HTTP Telemetry / Weather / Crypto / NTP)
 * ============================================================================ */
#ifndef APP_WIDGETS_H
#define APP_WIDGETS_H

#include "wm.h"
#include <stdint.h>

#define WIDGET_PIN_WEATHER   (1 << 0)
#define WIDGET_PIN_CRYPTO    (1 << 1)
#define WIDGET_PIN_CLOCK     (1 << 2)
#define WIDGET_PIN_SYSINFO   (1 << 3)

void widgets_init(void);
void widgets_update(int dt_ms);
void widgets_draw_window(struct window *win, int cx, int cy, int cw, int ch);
void widgets_mouse_click(struct window *win, int mx, int my, int button);
struct window *widgets_open_window(void);

/* Desktop background widget overlay renderer (if active) */
void widgets_draw_desktop_overlay(void);
int  widgets_handle_desktop_click(int mx, int my);

#endif
