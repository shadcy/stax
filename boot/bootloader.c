#include "memory_map.h"

#define UART0_BASE   0x101f1000UL
#define UART_DR      (*(volatile unsigned int *)(UART0_BASE + 0x000))
#define UART_FR      (*(volatile unsigned int *)(UART0_BASE + 0x018))
#define UART_IBRD    (*(volatile unsigned int *)(UART0_BASE + 0x024))
#define UART_FBRD    (*(volatile unsigned int *)(UART0_BASE + 0x028))
#define UART_LCRH    (*(volatile unsigned int *)(UART0_BASE + 0x02C))
#define UART_CR      (*(volatile unsigned int *)(UART0_BASE + 0x030))

static void uart_init(void) {
    UART_CR = 0; UART_IBRD = 13; UART_FBRD = 1;
    UART_LCRH = (0x3 << 5) | (1 << 4);
    UART_CR = (1 << 0) | (1 << 8) | (1 << 9);
}

static void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') {
            while (UART_FR & (1 << 5));
            UART_DR = '\r';
        }
        while (UART_FR & (1 << 5));
        UART_DR = *s++;
    }
}

void bootloader_main(void) {
    uart_init();
    uart_puts("--------------------------------------------------\n");
    uart_puts("  TIOS Bootloader v0.2 [3-Stage MBR-BL-K]\n");
    uart_puts("--------------------------------------------------\n");
    uart_puts("Loading kernel...\n");

    /* Copy Kernel from SRC to EXEC */
    unsigned int *src = (unsigned int *)KERNEL_SRC_ADDR;
    unsigned int *dst = (unsigned int *)KERNEL_EXEC_ADDR;
    unsigned int size = KERNEL_MAX_SIZE;
    while (size > 0) {
        *dst++ = *src++;
        size -= 4;
    }

    uart_puts("Jumping to kernel...\n\n");
    void (*kernel_entry)(void) = (void (*)(void))KERNEL_EXEC_ADDR;
    kernel_entry();
}
