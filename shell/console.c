/* ============================================================================
 * TIOS — console.c
 * Simple console output functions (dual output: UART + Framebuffer)
 * ============================================================================ */

#include "console.h"
#include "gfx_console.h"
#include "keyboard.h"

#define UART0_BASE  0x101f1000UL
#define UART_DR     (*(volatile unsigned int *)(UART0_BASE + 0x000))
#define UART_FR     (*(volatile unsigned int *)(UART0_BASE + 0x018))
#define UART_FR_TXFF (1 << 5)
#define UART_FR_RXFE (1 << 4)

void kputc(char c)
{
    /* Output to UART */
    if (c == '\n') {
        while (UART_FR & UART_FR_TXFF);
        UART_DR = '\r';
    }
    while (UART_FR & UART_FR_TXFF);
    UART_DR = (unsigned int)c;
    
    /* Also output to graphical console */
    gfx_putc(c);
}

void kputs(const char *s)
{
    while (*s) kputc(*s++);
}

void kput_uint(unsigned int n) {
    if (n == 0) {
        kputc('0');
        return;
    }
    char buf[16];
    int i = 0;
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }
    while (i > 0) {
        kputc(buf[--i]);
    }
}

void kput_hex(unsigned int n, int width) {
    char buf[16];
    int i = 0;
    do {
        int rem = n % 16;
        buf[i++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'a');
        n /= 16;
    } while (n > 0);
    while (i < width) buf[i++] = '0';
    while (i > 0) kputc(buf[--i]);
}

#include <stdarg.h>
void kprintf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    while (*format) {
        if (*format == '%') {
            format++;
            int width = 0;
            if (*format == '0') {
                format++;
                while (*format >= '0' && *format <= '9') {
                    width = width * 10 + (*format - '0');
                    format++;
                }
            }
            if (*format == 's') {
                char *s = va_arg(args, char *);
                if (s) kputs(s);
            } else if (*format == 'd' || *format == 'u') {
                unsigned int n = va_arg(args, unsigned int);
                kput_uint(n);
            } else if (*format == 'x' || *format == 'X') {
                unsigned int n = va_arg(args, unsigned int);
                kput_hex(n, width);
            } else if (*format == 'c') {
                char c = (char)va_arg(args, int);
                kputc(c);
            } else if (*format == '%') {
                kputc('%');
            } else {
                kputc('%');
                if (width) kputc('0');
                kputc(*format);
            }
        } else {
            kputc(*format);
        }
        format++;
    }
    va_end(args);
}

char kgetc(void)
{
    // UART input
    if (!(UART_FR & UART_FR_RXFE)) {
        char c = (char)UART_DR;
        if (c != 0) return c;
    }
    // PS/2 keyboard fallback
    return kb_getc();
}

void kgets(char *buf, int max_len)
{
    int i = 0;
    char c;
    volatile int timeout;
    
    /* Validate input parameters */
    if (!buf || max_len <= 0) return;
    
    while (i < max_len - 1) {
        /* Poll keyboard + UART; also keep network stack alive */
        extern int net_poll(void);
        net_poll();

        c = kgetc();
        if (c == 0) {
            /* No character yet — spin briefly and retry */
            for (volatile int k = 0; k < 50000; k++) __asm__ volatile ("nop");
            continue;
        }
        
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
