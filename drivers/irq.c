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
    
    /* Copy the vector table (first 8 instructions + 8 literal pool entries = 64 bytes)
       to 0x00000000 using inline assembly to avoid GCC's null-pointer UB optimization. */
    asm volatile (
        "mov r0, #0\n"
        "mov r1, %0\n"
        "ldm r1!, {r2-r9}\n"
        "stm r0!, {r2-r9}\n"
        "ldm r1!, {r2-r9}\n"
        "stm r0!, {r2-r9}\n"
        : : "r"(vector_table) : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "memory"
    );

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
