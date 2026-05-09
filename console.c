/* ============================================================================
 * TIOS — console.c
 * Simple console output functions
 * ============================================================================ */

#include "console.h"

#define UART0_BASE  0x101f1000UL
#define UART_DR     (*(volatile unsigned int *)(UART0_BASE + 0x000))
#define UART_FR     (*(volatile unsigned int *)(UART0_BASE + 0x018))
#define UART_FR_TXFF (1 << 5)
#define UART_FR_RXFE (1 << 4)

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

char kgetc(void)
{
    /* Check if RX FIFO has data */
    if (UART_FR & UART_FR_RXFE) {
        return 0;  /* No data available */
    }
    return (char)UART_DR;
}

void kgets(char *buf, int max_len)
{
    int i = 0;
    char c;
    volatile int timeout;
    
    /* Validate input parameters */
    if (!buf || max_len <= 0) return;
    
    while (i < max_len - 1) {
        /* Wait for character with timeout */
        timeout = 1000000;  /* Large timeout value */
        while (timeout-- && (UART_FR & UART_FR_RXFE)) {
            __asm__ volatile ("nop");
        }
        
        if (timeout <= 0) {
            /* Timeout - just continue loop */
            continue;
        }
        
        c = kgetc();
        if (c == 0) continue;  /* No valid character */
        
        /* Handle backspace */
        if (c == '\b' || c == 127) {
            if (i > 0) {
                i--;
                kputc('\b');
                kputc(' ');
                kputc('\b');
            }
            continue;
        }
        
        /* Handle enter/return */
        if (c == '\r' || c == '\n') {
            buf[i] = '\0';
            kputc('\n');
            return;
        }
        
        /* Only store printable characters */
        if (c >= 32 && c <= 126) {
            kputc(c);
            buf[i++] = c;
        }
    }
    
    buf[max_len - 1] = '\0';
    kputc('\n');
}
