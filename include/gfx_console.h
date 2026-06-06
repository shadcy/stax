/* ============================================================================
 * TIOS — gfx_console.h
 * ============================================================================ */
#ifndef GFX_CONSOLE_H
#define GFX_CONSOLE_H

#include <stdint.h>

int  gfx_console_init(void);
void gfx_putc(char c);
void gfx_puts(const char *s);
void gfx_puts_color(const char *s, uint16_t color);
void gfx_set_color(uint16_t color);
void gfx_clear(void);
void gfx_console_enable(int enable);

/* Call from the main idle loop to drive the cursor blink */
void gfx_tick(void);

struct window;
void gfx_console_draw_window(struct window *win, int cx, int cy, int cw, int ch);

#endif
