/* ============================================================================
 * STAX — console.c
 * Simple console output functions (dual output: UART + Framebuffer)
 * ============================================================================ */

#include "console.h"
#include "gfx_console.h"
#include "keyboard.h"
#include <stddef.h>

#define UART0_BASE  0x101f1000UL
#define UART_DR     (*(volatile unsigned int *)(UART0_BASE + 0x000))
#define UART_FR     (*(volatile unsigned int *)(UART0_BASE + 0x018))
#define UART_FR_TXFF (1 << 5)
#define UART_FR_RXFE (1 << 4)

static console_hook_fn g_console_hook = NULL;
static void *g_console_hook_ctx = NULL;

void console_set_hook(console_hook_fn fn, void *ctx) {
    g_console_hook = fn;
    g_console_hook_ctx = ctx;
}

void kputc(char c)
{
    /* Output to UART */
    if (c == '\n') {
        while (UART_FR & UART_FR_TXFF);
        UART_DR = '\r';
    }
    while (UART_FR & UART_FR_TXFF);
    UART_DR = (unsigned int)c;
    
    /* Output to active GUI terminal window hook if redirected */
    if (g_console_hook) {
        g_console_hook(c, g_console_hook_ctx);
    } else {
        /* Otherwise output to graphical boot console */
        gfx_putc(c);
    }
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
    /* 1. Check UART RX first (works for: make qemu / serial terminal) */
    if (!(UART_FR & UART_FR_RXFE))
        return (char)UART_DR;

    /* 2. Fall back to PL050 PS/2 keyboard (works for: make qemu-gfx window) */
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

#include <stdarg.h>

void kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const char *p = fmt;
    while (*p) {
        if (*p == '%') {
            p++;
            if (*p == 's') {
                char *s = va_arg(args, char *);
                if (s) kputs(s); else kputs("(null)");
            } else if (*p == 'u' || *p == 'd') {
                unsigned int val = va_arg(args, unsigned int);
                kput_uint(val);
            } else if (*p == 'p') {
                void *ptr = va_arg(args, void *);
                unsigned int val = (unsigned int)ptr;
                char buf[16];
                int i = 0;
                kputs("0x");
                if (val == 0) { kputc('0'); }
                else {
                    while (val > 0) {
                        int r = val % 16;
                        buf[i++] = (r < 10) ? ('0' + r) : ('a' + r - 10);
                        val /= 16;
                    }
                    while (i > 0) kputc(buf[--i]);
                }
            } else if (*p == 'x' || *p == 'X') {
                unsigned int val = va_arg(args, unsigned int);
                char buf[16];
                int i = 0;
                if (val == 0) { kputc('0'); }
                else {
                    while (val > 0) {
                        int rem = val % 16;
                        buf[i++] = (rem < 10) ? ('0' + rem) : ('a' + rem - 10);
                        val /= 16;
                    }
                    while (i > 0) kputc(buf[--i]);
                }
            } else if (*p == '0' && *(p+1) == '2' && *(p+2) == 'x') {
                p += 2;
                unsigned int val = va_arg(args, unsigned int);
                char buf[2];
                buf[0] = (val/16 < 10) ? ('0' + val/16) : ('a' + val/16 - 10);
                buf[1] = (val%16 < 10) ? ('0' + val%16) : ('a' + val%16 - 10);
                kputc(buf[0]); kputc(buf[1]);
            } else if (*p == 'c') {
                char c = (char)va_arg(args, int);
                kputc(c);
            } else {
                kputc('%');
                kputc(*p);
            }
        } else {
            kputc(*p);
        }
        p++;
    }
    va_end(args);
}
