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

/* External variables */
extern volatile unsigned int tick_count;

/* Command table */
static const command_t commands[] = {
    {"help", "Show available commands", cmd_help},
    {"clear", "Clear screen", cmd_clear},
    {"status", "Show system status", cmd_status},
    {"mem", "Show memory information", cmd_mem},
    {"tasks", "Show task information", cmd_tasks},
    {"fs", "Show filesystem information", cmd_fs},
    {"test", "Run system tests", cmd_test},
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
    kputs("\033[2J\033[H");  /* ANSI clear screen and home cursor */
}

void cmd_status(int argc, char *argv[])
{
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
    kputs("Memory Information:\n");
    kputs("==================\n");
    heap_stats();
}

void cmd_tasks(int argc, char *argv[])
{
    kputs("Task Information:\n");
    kputs("================\n");
    kputs("Scheduler: Round-robin\n");
    kputs("Current tasks: Idle + any created tasks\n");
    kputs("Use 'test' command to create demo tasks\n");
}

void cmd_fs(int argc, char *argv[])
{
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
