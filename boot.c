/* =============================================================================
 * TIOS — boot.c
 * Bare-metal bootloader for the ARM Versatile Platform Baseboard (versatilepb).
 *
 * What this file does:
 *   - Drives the PL011 UART directly via Memory-Mapped I/O (MMIO)
 *   - Prints a boot banner and memory map summary
 *   - Transfers control to kernel_main() in kernel.c
 *
 * There is NO libc here. No printf, no malloc, no OS of any kind.
 * Every byte written to the screen is written by the code below.
 * =============================================================================
 */

/* ---------------------------------------------------------------------------
 * UART register base address for PL011 UART0 on versatilepb.
 *
 * The 'volatile' keyword is critical:
 *   It tells the compiler "this memory address may change at any time due to
 *   hardware". Without it, the compiler might optimise away repeated reads or
 *   cache the value in a register — which would silently break MMIO.
 * ---------------------------------------------------------------------------
 */
#define UART0_BASE   0x101f1000UL

/* PL011 register offsets (in bytes from base address) */
#define UART_DR      (*(volatile unsigned int *)(UART0_BASE + 0x000)) /* Data Register      */
#define UART_FR      (*(volatile unsigned int *)(UART0_BASE + 0x018)) /* Flag Register      */
#define UART_IBRD    (*(volatile unsigned int *)(UART0_BASE + 0x024)) /* Integer Baud Rate  */
#define UART_FBRD    (*(volatile unsigned int *)(UART0_BASE + 0x028)) /* Fractional Baud    */
#define UART_LCRH    (*(volatile unsigned int *)(UART0_BASE + 0x02C)) /* Line Control       */
#define UART_CR      (*(volatile unsigned int *)(UART0_BASE + 0x030)) /* Control Register   */

/* UART Flag Register bits */
#define UART_FR_TXFF  (1 << 5)   /* Transmit FIFO Full */
#define UART_FR_RXFE  (1 << 4)   /* Receive  FIFO Empty */

/* UART Line Control bits */
#define UART_LCRH_WLEN_8  (0x3 << 5) /* 8-bit word length */
#define UART_LCRH_FEN     (1 << 4)   /* Enable FIFO       */

/* UART Control bits */
#define UART_CR_UARTEN (1 << 0)  /* UART enable  */
#define UART_CR_TXE    (1 << 8)  /* TX enable    */
#define UART_CR_RXE    (1 << 9)  /* RX enable    */

/* ---------------------------------------------------------------------------
 * Linker-script symbols
 * These are NOT variables — they are addresses injected by the linker.
 * Declared as arrays so we can take their address with '&' or cast directly.
 * ---------------------------------------------------------------------------
 */
extern unsigned int __bss_start[];
extern unsigned int __bss_end[];
extern unsigned int _text_start[];
extern unsigned int _text_end[];

/* ---------------------------------------------------------------------------
 * Forward declarations
 * ---------------------------------------------------------------------------
 */
static void uart_init(void);
static void uart_putc(char c);
static void uart_puts(const char *s);
static void uart_put_hex(unsigned int n);
static void uart_put_uint(unsigned int n);
extern void kernel_main(void);   /* defined in kernel.c */

/* ===========================================================================
 * uart_init — configure PL011 UART0 for 115200 8N1
 *
 * For a 24 MHz UART clock:
 *   BRD = 24,000,000 / (16 × 115200) = 13.0208...
 *   IBRD = 13
 *   FBRD = round(0.0208 × 64) = 1
 * ===========================================================================
 */
static void uart_init(void)
{
    UART_CR   = 0;                              /* Disable UART during config  */
    UART_IBRD = 13;                             /* Integer  baud rate divisor  */
    UART_FBRD = 1;                              /* Fractional baud rate        */
    UART_LCRH = UART_LCRH_WLEN_8 | UART_LCRH_FEN; /* 8-bit, FIFO on         */
    UART_CR   = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE; /* Enable       */
}

/* ===========================================================================
 * uart_putc — transmit one character, blocking until the TX FIFO has room
 * ===========================================================================
 */
static void uart_putc(char c)
{
    /* Carriage-return expansion: '\n' → "\r\n" for serial terminals */
    if (c == '\n') {
        while (UART_FR & UART_FR_TXFF);  /* wait while TX FIFO is full */
        UART_DR = '\r';
    }
    while (UART_FR & UART_FR_TXFF);
    UART_DR = (unsigned int)c;
}

/* ===========================================================================
 * uart_puts — transmit a null-terminated string
 * ===========================================================================
 */
static void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

/* ===========================================================================
 * uart_put_hex — print an unsigned int as "0xXXXXXXXX"
 * ===========================================================================
 */
static void uart_put_hex(unsigned int n)
{
    const char hex_chars[] = "0123456789ABCDEF";
    int i;
    uart_puts("0x");
    for (i = 28; i >= 0; i -= 4) {
        uart_putc(hex_chars[(n >> i) & 0xF]);
    }
}

/* ===========================================================================
 * uart_put_uint — print an unsigned int as decimal
 * ===========================================================================
 */
static void uart_put_uint(unsigned int n)
{
    char buf[12];
    int  i = 0;

    if (n == 0) { uart_putc('0'); return; }

    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    /* digits are in reverse order */
    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

/* ===========================================================================
 * print_divider — visual separator line
 * ===========================================================================
 */
static void print_divider(void)
{
    uart_puts("--------------------------------------------------\n");
}

/* ===========================================================================
 * boot_main — called by startup.s after stack and BSS are ready
 *
 * This is where C execution begins.
 * ===========================================================================
 */
void boot_main(void)
{
    /* ── 1. Initialise UART so we can print ──────────────────────────────── */
    uart_init();

    /* ── 2. Boot banner ──────────────────────────────────────────────────── */
    print_divider();
    uart_puts("  TIOS Bootloader v0.1\n");
    uart_puts("  Target : ARM Versatile Platform Baseboard\n");
    uart_puts("  UART   : PL011 @ ");
    uart_put_hex(UART0_BASE);
    uart_puts(", 115200 8N1\n");
    print_divider();

    /* ── 3. Print memory map ─────────────────────────────────────────────── */
    uart_puts("Memory map:\n");
    uart_puts("  .text  : ");
    uart_put_hex((unsigned int)_text_start);
    uart_puts(" -> ");
    uart_put_hex((unsigned int)_text_end);
    uart_putc('\n');

    uart_puts("  .bss   : ");
    uart_put_hex((unsigned int)__bss_start);
    uart_puts(" -> ");
    uart_put_hex((unsigned int)__bss_end);
    uart_putc('\n');

    uart_puts("  BSS sz : ");
    uart_put_uint((unsigned int)((char *)__bss_end - (char *)__bss_start));
    uart_puts(" bytes\n");
    print_divider();

    /* ── 4. Hand off to the kernel ───────────────────────────────────────── */
    uart_puts("Jumping to kernel...\n\n");
    kernel_main();

    /* ── 5. Should never reach here ─────────────────────────────────────── */
    uart_puts("\n[BOOT] kernel_main() returned — halting.\n");
    while (1);
}