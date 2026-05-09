/* =============================================================================
 * TIOS — kernel.c
 * The kernel entry point. boot_main() in boot.c calls kernel_main() here
 * after the stack is set up, BSS is zeroed, and the UART banner is printed.
 *
 * This is where YOUR operating system begins.
 *
 * Current state: Phase 5 skeleton.
 *   The kernel prints a status line over UART, then spins in an idle loop.
 *   Future phases will add interrupts, scheduling, memory allocation, etc.
 * =============================================================================
 */

/* ---------------------------------------------------------------------------
 * UART0 — same base address as boot.c.
 * In a real OS you'd call the driver from a shared uart.c / uart.h pair.
 * Repeated here to keep kernel.c self-contained and easy to read.
 * ---------------------------------------------------------------------------
 */
#define UART0_BASE  0x101f1000UL
#define UART_DR     (*(volatile unsigned int *)(UART0_BASE + 0x000))
#define UART_FR     (*(volatile unsigned int *)(UART0_BASE + 0x018))
#define UART_FR_TXFF (1 << 5)

/* ---------------------------------------------------------------------------
 * Simple kernel-side UART helpers
 * (boot.c already initialised the UART, so we just write to it)
 * ---------------------------------------------------------------------------
 */
static void kputc(char c)
{
    if (c == '\n') {
        while (UART_FR & UART_FR_TXFF);
        UART_DR = '\r';
    }
    while (UART_FR & UART_FR_TXFF);
    UART_DR = (unsigned int)c;
}

static void kputs(const char *s)
{
    while (*s) kputc(*s++);
}

/* ---------------------------------------------------------------------------
 * Kernel global state
 * These live in .bss (zeroed by startup.s before boot_main() runs).
 * ---------------------------------------------------------------------------
 */
static unsigned int tick_count  = 0;  /* incremented in future timer ISR    */
static unsigned int task_count  = 0;  /* number of tasks (scheduler: TODO)  */

/* ---------------------------------------------------------------------------
 * kernel_main — called by boot.c after boot is complete
 *
 * This function must never return.
 * ---------------------------------------------------------------------------
 */
void kernel_main(void)
{
    kputs("========================================\n");
    kputs("  TIOS Kernel — Phase 5\n");
    kputs("========================================\n");
    kputs("Status : running\n");
    kputs("Tasks  : 0 (scheduler not yet implemented)\n");
    kputs("IRQs   : disabled (vector table not yet set)\n");
    kputs("Heap   : not initialised\n");
    kputs("----------------------------------------\n");
    kputs("Next steps:\n");
    kputs("  Phase 6a — set up IRQ vector table\n");
    kputs("  Phase 6b — enable timer interrupt\n");
    kputs("  Phase 6c — add simple round-robin scheduler\n");
    kputs("  Phase 6d — add slab/bump memory allocator\n");
    kputs("  Phase 6e — add FAT filesystem driver\n");
    kputs("========================================\n");
    kputs("Kernel idle loop — halting CPU between ticks.\n\n");

    /*
     * Idle loop.
     *
     * In a real kernel this would execute a 'WFI' (Wait For Interrupt)
     * instruction to halt the CPU until the next timer IRQ fires,
     * then the scheduler would pick the next runnable task.
     *
     * For now we just spin and count ticks to prove we're alive.
     */
    while (1) {
        /* placeholder: future timer IRQ will increment tick_count */
        tick_count++;

        /*
         * WFI — Wait For Interrupt (ARM instruction).
         * Saves power; CPU resumes when any interrupt arrives.
         * Uncomment this once interrupts are configured in Phase 6b:
         *
         *   __asm__ volatile ("wfi");
         */

        /* suppress unused-variable warning until scheduler is added */
        (void)task_count;
    }
}