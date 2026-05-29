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
#include "framebuffer.h"
#include "bmp.h"
#include "gfx_console.h"
#include "string.h"

/* External variables */
extern volatile unsigned int tick_count;

#define MAX_COMMANDS 64
static command_t commands[MAX_COMMANDS];
static int num_commands = 0;

int command_register(const char *name, const char *desc, cmd_func_t func) {
    if (num_commands >= MAX_COMMANDS) {
        return -1; // Registry full
    }
    
    // Check for duplicates
    for (int i = 0; i < num_commands; i++) {
        if (strcmp(commands[i].name, name) == 0) {
            return -1; // Already registered
        }
    }
    
    commands[num_commands].name = name;
    commands[num_commands].desc = desc;
    commands[num_commands].func = func;
    num_commands++;
    return 0;
}



/* ------------------------------------------------------------------------
 * Command Parsing and Execution
 * ------------------------------------------------------------------------ */

static int parse_args(char *input, char *argv[], int max_args)
{
    int argc = 0;
    char *p = input;
    
    /* Skip leading whitespace */
    while (*p == ' ' || *p == '\t') p++;
    
    while (*p && argc < max_args - 1) {
        argv[argc++] = p;
        
        /* Find end of current argument */
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;
        
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
static void print_fat_error(FRESULT res) {
    switch (res) {
        case FR_OK: kputs("OK"); break;
        case FR_DISK_ERR: kputs("Disk error (Hardware/Timeout)"); break;
        case FR_INT_ERR: kputs("Internal assertion failed"); break;
        case FR_NOT_READY: kputs("Disk not ready"); break;
        case FR_NO_FILE: kputs("File not found"); break;
        case FR_NO_PATH: kputs("Path not found"); break;
        case FR_INVALID_NAME: kputs("Invalid name"); break;
        case FR_DENIED: kputs("Access denied / Read-only / Dir not empty"); break;
        case FR_EXIST: kputs("Already exists"); break;
        case FR_INVALID_OBJECT: kputs("Invalid object"); break;
        case FR_WRITE_PROTECTED: kputs("Write protected"); break;
        case FR_INVALID_DRIVE: kputs("Invalid drive"); break;
        case FR_NOT_ENABLED: kputs("Drive not mounted"); break;
        case FR_NO_FILESYSTEM: kputs("No filesystem"); break;
        case FR_LOCKED: kputs("File locked"); break;
        case FR_NOT_ENOUGH_CORE: kputs("Not enough memory"); break;
        case FR_TOO_MANY_OPEN_FILES: kputs("Too many open files"); break;
        case FR_INVALID_PARAMETER: kputs("Invalid parameter"); break;
        default: kputs("Unknown error"); break;
    }
}

void cmd_help(int argc, char *argv[])
{
    (void)argc; (void)argv;
    kputs("Available commands:\n");
    kputs("================================================\n");
    kputs("  COMMAND    | DESCRIPTION\n");
    kputs("------------------------------------------------\n");
    
    for (int i = 0; i < num_commands; i++) {
        kputs("  ");
        gfx_set_color(COLOR_GREEN); kputs("\x1b[32m");
        kputs(commands[i].name);
        
        int len = strlen(commands[i].name);
        for (int j = len; j < 10; j++) kputc(' ');
        
        gfx_set_color(COLOR_WHITE); kputs("\x1b[0m");
        kputs(" | ");
        
        /* Highlight options in description if they start with -- */
        const char *desc = commands[i].desc;
        while (*desc) {
            if (*desc == '-' && *(desc+1) == '-') {
                gfx_set_color(COLOR_MAGENTA); kputs("\x1b[35m");
                kputc(*desc++);
                kputc(*desc++);
                while (*desc && *desc != ',' && *desc != ' ' && *desc != ')') {
                    kputc(*desc++);
                }
                gfx_set_color(COLOR_WHITE); kputs("\x1b[0m");
            } else {
                kputc(*desc++);
            }
        }
        kputc('\n');
    }
    kputs("================================================\n");
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
    kputs("================================================\n");
    
    DWORD fre_clust, fre_sect, tot_sect;
    FATFS *fs;
    FRESULT res;
    
    res = f_getfree("", &fre_clust, &fs);
    if (res != FR_OK) {
        kputs("Failed to get filesystem info (");
        print_fat_error(res);
        kputs(")\n");
        return;
    }
    
    const char *fs_type = "Unknown";
    if (fs->fs_type == FS_FAT12) fs_type = "FAT12";
    else if (fs->fs_type == FS_FAT16) fs_type = "FAT16";
    else if (fs->fs_type == FS_FAT32) fs_type = "FAT32";
    else if (fs->fs_type == FS_EXFAT) fs_type = "exFAT";
    
    kputs("FAT Type     : "); kputs(fs_type); kputs("\n");
    
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;
    
    kputs("Total Space  : "); kput_uint(tot_sect / 2); kputs(" KB\n");
    kputs("Free Space   : "); kput_uint(fre_sect / 2); kputs(" KB\n");
    kputs("================================================\n");
}

void cmd_test(int argc, char *argv[])
{
    if (argc > 1) {
        if (strcmp(argv[1], "--fb") == 0) {
            cmd_fbtest(argc, argv);
            return;
        } else {
            kputs("Unknown test option.\n");
            return;
        }
    }
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
    if (argc > 1) {
        if (strcmp(argv[1], "--mem") == 0) {
            cmd_mem(argc, argv);
            return;
        } else if (strcmp(argv[1], "--img") == 0) {
            if (argc < 3) {
                kputs("Usage: ");
                gfx_set_color(COLOR_GREEN); kputs("\x1b[32mread ");
                gfx_set_color(COLOR_MAGENTA); kputs("\x1b[35m--img ");
                gfx_set_color(COLOR_WHITE); kputs("\x1b[0m<filename.bmp>\n");
                return;
            }
            char *new_argv[] = {"viewimg", argv[2], NULL};
            cmd_viewimg(2, new_argv);
            return;
        } else {
            kputs("Unknown read option.\n");
            return;
        }
    }
    
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
    
    extern volatile int fs_abort_flag;
    fs_abort_flag = 0;
    
    /* Validate input */
    if (!input) return;
    
    /* Check for empty input */
    if (input[0] == '\0') return;
    
    /* Limit input length to prevent buffer overflow (HIST_LEN = 64) */
    int input_len = 0;
    while (input[input_len] != '\0' && input_len < 63) input_len++;
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
    for (int i = 0; i < num_commands; i++) {
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
    command_register("help",    "Show available commands",           cmd_help);
    command_register("clear",   "Clear screen",                        cmd_clear);
    command_register("status",  "Show system status",                  cmd_status);
    command_register("tasks",   "Show task information",               cmd_tasks);
    command_register("fs",      "Show filesystem information",          cmd_fs);
    command_register("ls",      "List dir contents (use --size for showing size)", cmd_ls);
    command_register("cd",      "Change dir", cmd_cd);
    command_register("pwd",     "Print working dir", cmd_pwd);
    command_register("touch",   "Create empty file", cmd_touch);
    command_register("rm",      "Remove file or dir", cmd_rm);
    command_register("cat",     "Print file contents", cmd_cat);
    command_register("mkdir",   "Create dir", cmd_mkdir);
    command_register("nano",    "Edit text file (ESC to save & quit)", cmd_nano);
    command_register("read",    "Read info (use --mem, --img <img>)", cmd_read);
    command_register("test",    "Run tests (use --fb)", cmd_test);
    
    kputs("Command system initialized\n");
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

/* ============================================================================
 * cmd_viewimg — launch the image viewer
 * ============================================================================ */
void cmd_viewimg(int argc, char *argv[])
{
    if (argc < 2) {
        kputs("Usage: viewimg <filename.bmp>\n");
        return;
    }
    kputs("Loading image: ");
    kputs(argv[1]);
    kputs("\n");
    bmp_load_and_draw(argv[1], 0, 0);
    kputs("Image loaded. Press any key to continue...\n");
    while (kgetc() == 0);
    
    /* Re-initialize console to clear screen and restore the shell layout */
    gfx_console_init();
    kputs("========================================\n");
    kputs("  TIOS Kernel - back in shell\n");
    kputs("========================================\n");
    kputs("Type 'help' for available commands\n");
}

/* ============================================================================
 * Filesystem Commands (using FatFs thanks again fatfs module to save my weeks)
 * ============================================================================ */

void cmd_ls(int argc, char *argv[])
{
    int show_size = 0;
    const char *path = ".";
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--size") == 0 || strcmp(argv[i], "-s") == 0) {
            show_size = 1;
        } else {
            path = argv[i];
        }
    }

    DIR dir;
    FILINFO fno;
    FRESULT res;

    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        if (show_size) {
            kputs("Directory listing for ");
            kputs(path);
            kputs("\n--------------------------------\n");
        }
        
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break;
            
            if (show_size) {
                if (fno.fattrib & AM_DIR) {
                    kputs("<DIR>    ");
                } else {
                    kputs("         ");
                }
                kputs(fno.fname);
                
                if (!(fno.fattrib & AM_DIR)) {
                    kputs(" (");
                    kput_uint((unsigned int)fno.fsize);
                    kputs(" B)");
                }
                kputs("\n");
            } else {
                if (fno.fattrib & AM_DIR) {
                    gfx_set_color(COLOR_CYAN);
                    kputs("\x1b[36m");
                    kputs(fno.fname);
                    kputs("\x1b[0m");
                    gfx_set_color(COLOR_WHITE);
                } else {
                    kputs(fno.fname);
                }
                kputs("  ");
            }
        }
        f_closedir(&dir);

        if (show_size) {
            /* Show disk space */
            DWORD fre_clust, fre_sect, tot_sect;
            FATFS *fs;
            res = f_getfree(path, &fre_clust, &fs);
            if (res == FR_OK) {
                tot_sect = (fs->n_fatent - 2) * fs->csize;
                fre_sect = fre_clust * fs->csize;
                kputs("--------------------------------\n");
                kput_uint(tot_sect / 2); kputs(" KB ("); kput_uint((tot_sect / 2) / 1024); kputs(" MB) total drive space.\n");
                kput_uint(fre_sect / 2); kputs(" KB ("); kput_uint((fre_sect / 2) / 1024); kputs(" MB) available.\n");
            }
        } else {
            kputs("\n");
        }
    } else {
        kputs("Failed to open directory (");
        print_fat_error(res);
        kputs(")\n");
    }
}

void cmd_cd(int argc, char *argv[])
{
    if (argc < 2) {
        kputs("Usage: cd <path>\n");
        return;
    }
    FILINFO fno;
    FRESULT res = f_stat(argv[1], &fno);
    if (res == FR_OK && !(fno.fattrib & AM_DIR)) {
        kputs("cd: '"); kputs(argv[1]); kputs("' is not a directory.\n");
        return;
    }
    
    res = f_chdir(argv[1]);
    if (res != FR_OK) {
        kputs("Failed to change directory (");
        print_fat_error(res);
        kputs(")\n");
    }
}

void cmd_pwd(int argc, char *argv[])
{
    (void)argc; (void)argv;
    char path[256];
    FRESULT res = f_getcwd(path, sizeof(path));
    if (res == FR_OK) {
        kputs(path);
        kputs("\n");
    } else {
        kputs("Failed to get current directory\n");
    }
}

void cmd_touch(int argc, char *argv[])
{
    if (argc < 2) {
        kputs("Usage: touch <filename>\n");
        return;
    }
    
    FILINFO fno;
    FRESULT stat_res = f_stat(argv[1], &fno);
    if (stat_res == FR_OK) {
        if (fno.fattrib & AM_DIR) {
            kputs("touch: Cannot touch directory.\n");
            return;
        }
        if (fno.fattrib & AM_RDO) {
            kputs("touch: File is read-only.\n");
            return;
        }
    }
    
    FIL f;
    FRESULT res = f_open(&f, argv[1], FA_WRITE | FA_CREATE_ALWAYS);
    if (res == FR_OK) {
        f_close(&f);
        kputs("File created.\n");
    } else {
        kputs("Failed to create file (");
        print_fat_error(res);
        kputs(")\n");
    }
}

void cmd_rm(int argc, char *argv[])
{
    if (argc < 2) {
        kputs("Usage: rm <filename>\n");
        return;
    }
    
    FILINFO fno;
    FRESULT stat_res = f_stat(argv[1], &fno);
    if (stat_res != FR_OK) {
        kputs("rm: Cannot remove '"); kputs(argv[1]); kputs("': ");
        print_fat_error(stat_res); kputs("\n");
        return;
    }
    
    if (fno.fattrib & AM_RDO) {
        kputs("rm: Access denied, '"); kputs(argv[1]); kputs("' is read-only.\n");
        return;
    }
    
    FRESULT res = f_unlink(argv[1]);
    if (res == FR_OK) {
        kputs("Removed.\n");
    } else {
        kputs("Failed to remove (");
        print_fat_error(res);
        kputs(")\n");
    }
}

void cmd_cat(int argc, char *argv[])
{
    if (argc < 2) {
        kputs("Usage: cat <filename>\n");
        return;
    }
    
    FILINFO fno;
    FRESULT stat_res = f_stat(argv[1], &fno);
    if (stat_res == FR_OK && (fno.fattrib & AM_DIR)) {
        kputs("cat: '"); kputs(argv[1]); kputs("' is a directory.\n");
        return;
    }
    
    FIL f;
    FRESULT res = f_open(&f, argv[1], FA_READ);
    if (res != FR_OK) {
        kputs("Failed to open file (");
        print_fat_error(res);
        kputs(")\n");
        return;
    }
    
    char buf[128];
    UINT br;
    while (f_read(&f, buf, sizeof(buf) - 1, &br) == FR_OK && br > 0) {
        for (UINT i = 0; i < br; i++) {
            gfx_set_color(COLOR_GREEN);
            kputc(buf[i]);
        }
    }
    kputs("\n");
    f_close(&f);
}

void cmd_mkdir(int argc, char *argv[])
{
    if (argc < 2) {
        kputs("Usage: mkdir <dirname>\n");
        return;
    }
    
    FILINFO fno;
    if (f_stat(argv[1], &fno) == FR_OK) {
        kputs("mkdir: Cannot create directory '"); kputs(argv[1]); kputs("': File exists.\n");
        return;
    }
    
    FRESULT res = f_mkdir(argv[1]);
    if (res == FR_OK) {
        kputs("Directory created.\n");
    } else {
        kputs("Failed to create directory (");
        print_fat_error(res);
        kputs(")\n");
    }
}

void cmd_nano(int argc, char *argv[])
{
    if (argc < 2) {
        kputs("Usage: nano <filename>\n");
        return;
    }
    
    char *filename = argv[1];
    
    FILINFO fno;
    FRESULT stat_res = f_stat(filename, &fno);
    if (stat_res == FR_OK) {
        if (fno.fattrib & AM_DIR) {
            kputs("nano: Cannot edit directory.\n");
            return;
        }
        if (fno.fattrib & AM_RDO) {
            kputs("nano: File is read-only.\n");
            return;
        }
    }
    
    kputs("Starting nano editor for: ");
    kputs(filename);
    kputs("\nPress ESC to save and quit.\n");
    
    FIL f;
    FRESULT res = f_open(&f, filename, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
    if (res != FR_OK) {
        kputs("Failed to open file (");
        print_fat_error(res);
        kputs(")\n");
        return;
    }
    
    /* Allocate 8KB buffer */
    char *buf = kmalloc(8192);
    if (!buf) {
        kputs("Failed to allocate memory for nano\n");
        f_close(&f);
        return;
    }
    
    /* Read existing file */
    UINT br;
    res = f_read(&f, buf, 8192 - 1, &br);
    if (res != FR_OK) {
        kputs("Failed to read file\n");
        kfree(buf);
        f_close(&f);
        return;
    }
    
    int len = br;
    buf[len] = '\0';
    
    /* Clear console and print current buffer */
    gfx_clear();
    gfx_puts("--- Nano Editor (Press ESC to save & quit) ---\n");
    for (int i = 0; i < len; i++) {
        kputc(buf[i]);
    }
    
    /* Input loop */
    while (1) {
        char c = kgetc();
        if (c == 0) {
            for (volatile int i = 0; i < 50000; i++) __asm__ volatile ("nop");
            gfx_tick();
            continue;
        }
        
        if (c == '\x1b') { /* ESC to quit */
            break;
        }
        
        if (c == '\b' || c == 127) {
            if (len > 0) {
                len--;
                buf[len] = '\0';
                kputc('\b');
            }
        } else if (c == '\r' || c == '\n') {
            if (len < 8191) {
                buf[len++] = '\n';
                buf[len] = '\0';
                kputc('\n');
            }
        } else if (c >= 32 && c <= 126) {
            if (len < 8191) {
                buf[len++] = c;
                buf[len] = '\0';
                kputc(c);
            }
        }
    }
    
    /* Save to file */
    f_lseek(&f, 0);
    f_truncate(&f);
    
    UINT bw;
    res = f_write(&f, buf, len, &bw);
    f_close(&f);
    
    kfree(buf);
    
    gfx_clear();
    kputs("========================================\n");
    kputs("  TIOS Kernel - back in shell\n");
    kputs("========================================\n");
    if (res == FR_OK && bw == (UINT)len) {
        kputs("File saved successfully.\n");
    } else {
        kputs("Failed to save file completely.\n");
    }
    kputs("Type 'help' for available commands\n");
}
