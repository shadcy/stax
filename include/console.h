/* ============================================================================
 * STAX — console.h
 * Console function declarations
 * ============================================================================ */

#ifndef CONSOLE_H
#define CONSOLE_H

void kputc(char c);
void kputs(const char *s);
void kput_uint(unsigned int n);

/* Input functions */
char kgetc(void);
/* Output redirection hook */
typedef void (*console_hook_fn)(char c, void *ctx);
void console_set_hook(console_hook_fn fn, void *ctx);

#endif

void kprintf(const char *fmt, ...);

