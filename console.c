/* ============================================================================
 * TIOS — console.c
 * Simple console output functions
 * ============================================================================ */

#include "console.h"

#define UART0_BASE  0x101f1000UL
#define UART_DR     (*(volatile unsigned int *)(UART0_BASE + 0x000))
#define UART_FR     (*(volatile unsigned int *)(UART0_BASE + 0x018))
#define UART_FR_TXFF (1 << 5)

void kputc(char c)
{
    if (c == '\n') {
        while (UART_FR & UART_FR_TXFF);
        UART_DR = '\r';
    }
    while (UART_FR & UART_FR_TXFF);
    UART_DR = (unsigned int)c;
}

void kputs(const char *s)
{
    while (*s) kputc(*s++);
}

void kput_uint(unsigned int n)
{
    char buf[12];
    int i = 0;
    if (n == 0) { kputc('0'); return; }
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) kputc(buf[--i]);
}
