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
void kgets(char *buf, int max_len);

#endif
