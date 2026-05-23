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

#endif
