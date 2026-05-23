/* ============================================================================
 * TIOS — command.c
 * Interactive command system
 * ============================================================================ */

#include "command.h"
#include "console.h"
#include "heap.h"
#include "fat.h"
#include "scheduler.h"
#include "timer.h"
#include "snake.h"
#include "doom.h"
#include "framebuffer.h"

/* External variables */
extern volatile unsigned int tick_count;

/* Command table */
static const command_t commands[] = {
    {"help",    "Show available commands",           cmd_help},
    {"clear",   "Clear screen",                        cmd_clear},
    {"status",  "Show system status",                  cmd_status},
    {"mem",     "Show memory information",              cmd_mem},
    {"tasks",   "Show task information",               cmd_tasks},
    {"fs",      "Show filesystem information",          cmd_fs},
    {"test",    "Run system tests",                    cmd_test},
    {"read",    "Read system memory space info",        cmd_read},
    {"snake",   "Play Graphical Snake (WASD, Q to quit)", cmd_snake},
    {"doomgfx", "Play DOOM Graphics (WASD, Q to quit)", cmd_doomgfx},
    {"fbtest",  "Test framebuffer (graphics mode)",     cmd_fbtest},
    {NULL, NULL, NULL}  /* End marker */
};

/* Simple string comparison */
static int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/* Simple string length */
static size_t strlen(const char *s)
{
    size_t len = 0;
    while (*s++) len++;
    return len;
}

/* Parse command line into arguments */
static int parse_args(char *input, char *argv[], int max_args)
{
    int argc = 0;
    char *p = input;
    
    /* Skip leading whitespace */
    while (*p == ' ' || *p == '\t') p++;
    
    while (*p && argc < max_args - 1) {
        argv[argc++] = p;
        
        /* Find end of current argument */
        while (*p && *p != ' ' && *p != '\t') p++;
        
        if (*p) {
            *p++ = '\0';  /* Terminate current argument */
            /* Skip whitespace */
            while (*p == ' ' || *p == '\t') p++;
        }
    }
    
    argv[argc] = NULL;
    return argc;
}

/* Command implementations */
void cmd_help(int argc, char *argv[])
{
    (void)argc; (void)argv;
    kputs("Available commands:\n");
    kputs("==================\n");
    
    for (int i = 0; commands[i].name != NULL; i++) {
        kputs("  ");
        kputs(commands[i].name);
        kputs(" - ");
        kputs(commands[i].desc);
        kputc('\n');
    }
}

void cmd_clear(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    /* 1. Clear the serial UART terminal (ANSI escape) */
    #define UART0_BASE  0x101f1000UL
    #define UART_DR     (*(volatile unsigned int *)(UART0_BASE + 0x000))
    #define UART_FR     (*(volatile unsigned int *)(UART0_BASE + 0x018))
    #define UART_FR_TXFF (1 << 5)
    
    const char *ansi_clear = "\033[2J\033[H";
    while (*ansi_clear) {
        while (UART_FR & UART_FR_TXFF);
        UART_DR = (unsigned int)(*ansi_clear++);
    }
    
    /* 2. Clear the graphical console cleanly */
    extern void gfx_clear(void);
    gfx_clear();
}

void cmd_status(int argc, char *argv[])
{
    (void)argc; (void)argv;
    kputs("System Status:\n");
    kputs("=============\n");
    kputs("Kernel: TIOS Phase 6e\n");
    kputs("CPU: ARM926EJ-S\n");
    kputs("Board: VersatilePB\n");
    kputs("Uptime: ");
    kput_uint(tick_count / 10);  /* Convert ticks to seconds */
    kputs(" seconds\n");
    kputs("IRQs: Enabled\n");
    kputs("Timer: 10 Hz (100ms ticks)\n");
}

void cmd_mem(int argc, char *argv[])
{
    (void)argc; (void)argv;
    kputs("Memory Information:\n");
    kputs("==================\n");
    heap_stats();
}

void cmd_tasks(int argc, char *argv[])
{
    (void)argc; (void)argv;
    kputs("Task Information:\n");
    kputs("================\n");
    kputs("Scheduler: Round-robin\n");
    kputs("Current tasks: Idle + any created tasks\n");
    kputs("Use 'test' command to create demo tasks\n");
}

void cmd_fs(int argc, char *argv[])
{
    (void)argc; (void)argv;
    kputs("Filesystem Information:\n");
    kputs("======================\n");
    kputs("FAT12/16 driver active\n");
    kputs("Test image mounted\n");
    
    /* Try to list files */
    fat_file_t *file = fat_open("TEST.TXT");
    if (file) {
        kputs("TEST.TXT: Found and readable\n");
        fat_close(file);
    } else {
        kputs("TEST.TXT: Not found\n");
    }
}

void cmd_test(int argc, char *argv[])
{
    (void)argc; (void)argv;
    kputs("Running system tests...\n");
    
    /* Test memory allocation */
    char *buf1 = kmalloc(64);
    char *buf2 = kmalloc(128);
    if (buf1 && buf2) {
        kputs("✓ Memory allocation test passed\n");
        kfree(buf2);
        kfree(buf1);
    } else {
        kputs("✗ Memory allocation test failed\n");
    }
    
    /* Test filesystem */
    fat_file_t *file = fat_open("TEST.TXT");
    if (file) {
        kputs("✓ Filesystem test passed\n");
        char read_buf[32];
        int bytes = fat_read(file, read_buf, sizeof(read_buf) - 1);
        if (bytes > 0) {
            read_buf[bytes] = '\0';
            kputs("Content: ");
            kputs(read_buf);
            kputc('\n');
        }
        fat_close(file);
    } else {
        kputs("✗ Filesystem test failed\n");
    }
    
    kputs("Tests completed.\n");
}

extern unsigned char _text_start[];
extern unsigned char _text_end[];
extern unsigned char _data_start[];
extern unsigned char _data_end[];
extern unsigned char __bss_start[];
extern unsigned char __bss_end[];
extern unsigned char __heap_start[];
extern unsigned char __heap_end[];
extern unsigned char stack_top[];

static void print_size_optimal(unsigned int bytes) {
    if (bytes >= 1024 * 1024) {
        unsigned int mb = bytes / (1024 * 1024);
        unsigned int rem = (bytes % (1024 * 1024)) * 100 / (1024 * 1024);
        kput_uint(mb);
        kputc('.');
        if (rem < 10) kputc('0');
        kput_uint(rem);
        kputs(" MB (");
        kput_uint(bytes);
        kputs(" B)");
    } else if (bytes >= 1024) {
        unsigned int kb = bytes / 1024;
        unsigned int rem = (bytes % 1024) * 100 / 1024;
        kput_uint(kb);
        kputc('.');
        if (rem < 10) kputc('0');
        kput_uint(rem);
        kputs(" KB (");
        kput_uint(bytes);
        kputs(" B)");
    } else {
        kput_uint(bytes);
        kputs(" B");
    }
}

void cmd_read(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    unsigned int total_ram = 4 * 1024 * 1024; /* 4 MB as defined in linker script */
    
    /* Using actual linker boundary markers */
    unsigned int kernel_size = _text_end - _text_start;
    unsigned int data_size   = _data_end - _data_start;
    unsigned int bss_size    = __bss_end - __bss_start;
    
    unsigned int heap_size = __heap_end - __heap_start;
    unsigned int stack_size = 8192;
    
    unsigned int total_static = kernel_size + data_size + bss_size;
    unsigned int user_program_space = total_ram - total_static;
    unsigned int free_space = user_program_space - (heap_size + stack_size);
    
    kputs("System Memory Space Info:\n");
    kputs("=========================\n");
    
    kputs("Total Available RAM : ");
    print_size_optimal(total_ram);
    kputs("\n");
    
    kputs("Kernel Text Size    : ");
    print_size_optimal(kernel_size);
    kputs("\n");
    
    kputs("Kernel Data Size    : ");
    print_size_optimal(data_size);
    kputs("\n");
    
    kputs("BSS Size            : ");
    print_size_optimal(bss_size);
    kputs("\n");
    
    kputs("User Program Space  : ");
    print_size_optimal(user_program_space);
    kputs("\n");
    
    kputs("  |- Heap Space     : ");
    print_size_optimal(heap_size);
    kputs("\n");
    
    kputs("  |- Stack Space    : ");
    print_size_optimal(stack_size);
    kputs("\n");
    
    kputs("  |- Unallocated    : ");
    print_size_optimal(free_space);
    kputs("\n");
}

/* Main command processing */
void command_process(char *input)
{
    char *argv[8];  /* Support up to 7 arguments + command */
    int argc;
    
    /* Validate input */
    if (!input) return;
    
    /* Check for empty input */
    if (input[0] == '\0') return;
    
    /* Limit input length to prevent buffer overflow */
    int input_len = 0;
    while (input[input_len] != '\0' && input_len < 30) input_len++;
    input[input_len] = '\0';  /* Ensure null termination */
    
    argc = parse_args(input, argv, 8);
    
    if (argc == 0) return;  /* Empty line */
    
    /* Validate first argument */
    if (!argv[0]) return;
    
    /* Limit command name length */
    if (strlen(argv[0]) > 10) {
        kputs("Command too long\n");
        return;
    }
    
    /* Find and execute command */
    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            commands[i].func(argc, argv);
            return;
        }
    }
    
    /* Command not found */
    kputs("Command not found: ");
    kputs(argv[0]);
    kputs("\nType 'help' for available commands\n");
}

void command_init(void)
{
    kputs("Command system initialized\n");
}

/* ============================================================================
 * cmd_snake — launch the Snake game
 * ============================================================================ */
void cmd_snake(int argc, char *argv[])
{
    (void)argc; (void)argv;
    kputs("Starting Snake...\n");
    snake_run();
    
    gfx_console_init();
    
    kputs("========================================\n");
    kputs("  TIOS Kernel - back in shell\n");
    kputs("========================================\n");
    kputs("Type 'help' for available commands\n");
}

/* ============================================================================
 * cmd_doomgfx — launch the graphical DOOM game
 * ============================================================================ */
void cmd_doomgfx(int argc, char *argv[])
{
    (void)argc; (void)argv;
    kputs("Starting DOOM (Graphical version)...\n");
    kputs("This requires QEMU with -serial stdio\n");
    doom_gfx_run();
    
    /* Re-initialize console to clear screen and restore the shell layout */
    gfx_console_init();
    
    kputs("========================================\n");
    kputs("  TIOS Kernel - back in shell\n");
    kputs("========================================\n");
    kputs("Type 'help' for available commands\n");
}

/* ============================================================================
 * cmd_fbtest — test framebuffer
 * ============================================================================ */
void cmd_fbtest(int argc, char *argv[])
{
    (void)argc; (void)argv;
    kputs("Testing framebuffer...\n");
    
    /* Initialize framebuffer */
    if (fb_init() != 0) {
        kputs("Failed to initialize framebuffer!\n");
        kputs("Make sure you're running with: make qemu-gfx\n");
        return;
    }
    
    kputs("Framebuffer initialized successfully!\n");
    kputs("Drawing test pattern...\n");
    
    /* Draw test pattern */
    fb_clear(COLOR_BLACK);
    
    /* Draw colored rectangles */
    fb_fillrect(50, 50, 100, 100, COLOR_RED);
    fb_fillrect(200, 50, 100, 100, COLOR_GREEN);
    fb_fillrect(350, 50, 100, 100, COLOR_BLUE);
    
    fb_fillrect(50, 200, 100, 100, COLOR_YELLOW);
    fb_fillrect(200, 200, 100, 100, COLOR_CYAN);
    fb_fillrect(350, 200, 100, 100, COLOR_MAGENTA);
    
    fb_fillrect(125, 350, 250, 80, COLOR_WHITE);
    
    kputs("Test pattern drawn!\n");
    kputs("You should see colored rectangles on the display.\n");
    kputs("Press any key to continue...\n");
    
    /* Wait for key */
    while (kgetc() == 0);
    
    /* Clear screen */
    fb_clear(COLOR_BLACK);
    kputs("Framebuffer test complete.\n");
}
