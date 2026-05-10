/* ============================================================================
 * TIOS — irq.c
 * IRQ dispatcher and system initialisation
 * ============================================================================ */

#include "irq.h"
#include "vic.h"

#define UART0_BASE  0x101F1000UL
#define UART_DR     (*(volatile uint32_t *)(UART0_BASE + 0x000))
#define UART_FR     (*(volatile uint32_t *)(UART0_BASE + 0x018))
#define UART_FR_TXFF (1 << 5)

static void early_putc(char c)
{
    if (c == '\n') {
        while (UART_FR & UART_FR_TXFF);
        UART_DR = '\r';
    }
    while (UART_FR & UART_FR_TXFF);
    UART_DR = (uint32_t)c;
}

static void early_puts(const char *s)
{
    while (*s) early_putc(*s++);
}

void irq_dispatch(void)
{
    void (*handler)(void);
    handler = (void (*)(void))VICVECTADDR;
    if (handler) {
        handler();
    } else {
        early_puts("[IRQ] unhandled interrupt!\n");
    }
    vic_acknowledge();
}

void irq_system_init(void)
{
    extern uint32_t vector_table[];
    volatile uint32_t *vt = (volatile uint32_t *)0x00000000;
    int i;

    for (i = 0; i < 8; i++) {
        vt[i] = 0xe59ff018;
    }
    for (i = 0; i < 8; i++) {
        vt[8 + i] = vector_table[8 + i];
    }

    vic_init();
}

/* ============================================================================
 * irq_register — find a free slot, register handler, AND enable the VIC source.
 * ============================================================================ */
int irq_register(uint32_t source, void (*handler)(void))
{
    int slot;
    for (slot = 0; slot < VIC_NUM_SLOTS; slot++) {
        if (VICVECTCNTLn(slot) == 0) {
            int rc = vic_register_handler(slot, source, handler);
            if (rc == 0) {
                vic_enable_source(source);
            }
            return rc;
        }
    }
    return -1;
}
