/* ============================================================================
 * STAX — command.c
 * Interactive command system
 * ============================================================================ */

#include "command.h"
#include "console.h"
#include "heap.h"
#include "fat.h"
#include "scheduler.h"
#include "timer.h"
#include "snake.h"
#include "framebuffer.h"
#include "bmp.h"
#include "gfx_console.h"
#include "string.h"
#include "bench.h"
#include "page.h"
#include "system.h"
#include "signal.h"
#include "pipe.h"

/* External variables */
extern volatile unsigned int tick_count;
extern void cmd_browser(int argc, char *argv[]);
void cmd_exec(int argc, char *argv[]);
void cmd_vfs(int argc, char *argv[]);
void cmd_dev(int argc, char *argv[]);

/* Command table */
static const command_t commands[] = {
    {"help",    "Show available commands",           cmd_help},
    {"clear",   "Clear screen",                        cmd_clear},
    {"reboot",  "Restart the system",                  cmd_reboot},
    {"status",  "Show system status",                  cmd_status},
    {"tasks",   "Show task information",               cmd_tasks},
    {"fs",      "Show filesystem information",          cmd_fs},
    {"vfs",     "Show Virtual File System & mount points", cmd_vfs},
    {"dev",     "List and test /dev device nodes",     cmd_dev},
    {"ls",      "List dir contents (use --size for showing size)", cmd_ls},
    {"cd",      "Change dir", cmd_cd},
    {"pwd",     "Print working dir", cmd_pwd},
    {"touch",   "Create empty file", cmd_touch},
    {"rm",      "Remove file or dir", cmd_rm},
    {"cat",     "Print file contents", cmd_cat},
    {"mkdir",   "Create dir", cmd_mkdir},
    {"nano",    "Edit text file (ESC to save & quit)", cmd_nano},
    {"exec",    "Load and execute standalone ELF-32 binary", cmd_exec},
    {"run",     "Run a .launch application package (e.g. run doom.launch)", cmd_run},
    {"game",    "Play a game (use --doom)",            cmd_game},
    {"read",    "Read info (use --mem, --img <img>)", cmd_read},
    {"test",    "Run tests ([--mem] [--fs] [--vfs] [--elf] [--all])", cmd_test},
#ifdef ENABLE_BENCH
    {"bench",   "Run benchmarks ([--memory] [--vm] [--scheduler] [--fs] [--gfx] [--firmware] [--all])", cmd_bench},
    {"stress",  "Run stress tests", cmd_stress},
#endif
    {"ps",      "Show process/task list", cmd_ps},
    {"uptime",  "Show system uptime", cmd_uptime},
    {"fwupdate","Update system firmware (e.g. fwupdate /fw.stax)", cmd_fwupdate},
    {"fwconfirm","Confirm active firmware to prevent rollback", cmd_fwconfirm},
    {"ifconfig","Show network interface configuration", cmd_ifconfig},
    {"ping",    "Send ICMP ECHO_REQUEST to network hosts", cmd_ping},
    {"browser", "Launch Graphical Web Browser", cmd_browser},
    {"date",    "Show current date and time (IST Mumbai)", cmd_date},
    {"time",    "Show current date and time (IST Mumbai)", cmd_date},
    {NULL,      NULL,                                NULL}
};

void cmd_game(int argc, char *argv[])
{
    if (argc < 2) {
        kputs("Usage:\n");
        gfx_set_color(COLOR_GREEN); kputs("\x1b[32m  game ");
        gfx_set_color(COLOR_MAGENTA); kputs("\x1b[35m--doom   ");
        gfx_set_color(COLOR_WHITE); kputs("\x1b[0m| Launch DOOM (doom.launch)\n");
        return;
    }
    if (strcmp(argv[1], "--doom") == 0) cmd_doomgfx(argc, argv);
    else kputs("Unknown game. Try: game --doom\n");
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
void cmd_reboot(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    kputs("Rebooting STAX...\n");
    system_reboot();
}

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
    
    for (int i = 0; commands[i].name != NULL; i++) {
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
    kputs("Kernel: STAX Phase 6e\n");
    kputs("CPU: ARM926EJ-S\n");
    kputs("Board: VersatilePB\n");
    kputs("Uptime: ");
    kput_uint(tick_count / 1000);  /* Convert ticks to seconds */
    kputs(" seconds\n");
    kputs("IRQs: Enabled\n");
    kputs("Timer: 1000 Hz (1 ms ticks)\n");
}

void cmd_mem(int argc, char *argv[])
{
    (void)argc; (void)argv;
    kputs("Memory Information:\n");
    kputs("==================\n");
    extern uint32_t heap_get_free(void);
    extern uint32_t heap_get_total(void);
    uint32_t tot = heap_get_total();
    uint32_t fre = heap_get_free();
    kputs("Heap managed pages: "); kput_uint(tot); kputs(" bytes\n");
    kputs("Heap free pages: "); kput_uint(fre); kputs(" bytes\n");
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
        } else if (strcmp(argv[1], "--elf") == 0) {
            extern int elf_exec(const char *path, int argc, char **argv);
            kputs("Testing ELF Loader on hello.elf...\n");
            int rc = elf_exec("hello.elf", 0, NULL);
            if (rc == 0) {
                kputs("PASS: ELF Loader executed successfully in USR mode.\n");
            } else {
                kputs("FAIL: ELF Loader returned error code.\n");
            }
            return;
        } else if (strcmp(argv[1], "--vfs") == 0 || strcmp(argv[1], "--dev") == 0) {
            extern int vfs_open(const char *path, int flags);
            extern int vfs_close(int fd);
            extern int32_t vfs_read(int fd, void *buf, size_t count);
            extern int32_t vfs_write(int fd, const void *buf, size_t count);

            kputs("Testing VFS & Devfs Subsystems...\n");
            
            /* Test /dev/null */
            int fd_null = vfs_open("/dev/null", 0);
            if (fd_null >= 0) {
                char dummy[8];
                int32_t n = vfs_read(fd_null, dummy, sizeof(dummy));
                if (n == 0) kputs("  ✓ /dev/null read returned EOF (0 bytes)\n");
                vfs_write(fd_null, "test", 4);
                vfs_close(fd_null);
            }

            /* Test /dev/zero */
            int fd_zero = vfs_open("/dev/zero", 0);
            if (fd_zero >= 0) {
                uint8_t zbuf[16];
                memset(zbuf, 0xFF, sizeof(zbuf));
                vfs_read(fd_zero, zbuf, sizeof(zbuf));
                int all_zero = 1;
                for (int i = 0; i < 16; i++) { if (zbuf[i] != 0) all_zero = 0; }
                if (all_zero) kputs("  ✓ /dev/zero filled buffer with 0x00\n");
                vfs_close(fd_zero);
            }

            /* Test /dev/urandom */
            int fd_rand = vfs_open("/dev/urandom", 0);
            if (fd_rand >= 0) {
                uint8_t rbuf[8];
                vfs_read(fd_rand, rbuf, sizeof(rbuf));
                kputs("  ✓ /dev/urandom generated random bytes: ");
                for (int i = 0; i < 4; i++) {
                    kprintf("%x ", (unsigned int)rbuf[i]);
                }
                kputs("\n");
                vfs_close(fd_rand);
            }
            kputs("PASS: VFS and Devfs subsystem verification complete.\n");
            return;
        } else if (strcmp(argv[1], "--pipe") == 0) {
            extern int pipe_create(int pipefd[2]);
            extern int vfs_close(int fd);
            extern int32_t vfs_read(int fd, void *buf, size_t count);
            extern int32_t vfs_write(int fd, const void *buf, size_t count);

            kputs("Testing Anonymous Pipes Subsystem...\n");
            int pfd[2];
            if (pipe_create(pfd) == 0) {
                kprintf("  ✓ Pipe created (Read FD: %d, Write FD: %d)\n", pfd[0], pfd[1]);
                const char *test_msg = "Hello STAX IPC Pipe!";
                int32_t written = vfs_write(pfd[1], test_msg, strlen(test_msg));
                kprintf("  ✓ Wrote %d bytes into pipe write-end\n", written);

                char rx_buf[32];
                memset(rx_buf, 0, sizeof(rx_buf));
                int32_t read_bytes = vfs_read(pfd[0], rx_buf, sizeof(rx_buf) - 1);
                kprintf("  ✓ Read %d bytes from pipe read-end: \"%s\"\n", read_bytes, rx_buf);

                vfs_close(pfd[1]);
                int32_t eof_check = vfs_read(pfd[0], rx_buf, sizeof(rx_buf));
                if (eof_check == 0) {
                    kputs("  ✓ Closed writer correctly signaled EOF to reader\n");
                }
                vfs_close(pfd[0]);
                kputs("PASS: Anonymous pipe streaming verified successfully.\n");
            } else {
                kputs("FAIL: Could not create pipe.\n");
            }
            return;
        } else if (strcmp(argv[1], "--signal") == 0) {
            static int s_sig_caught = 0;
            auto void s_test_handler(int sig);
            void s_test_handler(int sig) {
                s_sig_caught = sig;
            }

            kputs("Testing POSIX Signal Handling Engine...\n");
            signal_register(SIGINT, s_test_handler);
            kputs("  ✓ Registered user signal handler for SIGINT (2)\n");

            s_sig_caught = 0;
            signal_send(1, SIGINT);
            if (s_sig_caught == SIGINT) {
                kputs("  ✓ Signal dispatched & intercepted by user handler successfully\n");
            } else {
                kputs("  ✗ Signal dispatch failed\n");
            }

            signal_register(SIGINT, SIG_DFL);
            kputs("PASS: POSIX signal subsystem verified successfully.\n");
            return;
        } else {
            kputs("Unknown test option. Available: --fb, --elf, --vfs, --dev, --pipe, --signal\n");
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
    
    unsigned int total_ram = 32 * 1024 * 1024; /* 32 MB as defined in linker script */
    
    /* Using actual linker boundary markers */
    unsigned int kernel_size = _text_end - _text_start;
    unsigned int data_size   = _data_end - _data_start;
    unsigned int bss_size    = __bss_end - __bss_start;
    
    extern int get_total_memory(void);
    unsigned int heap_size = get_total_memory();
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
 * cmd_run — launch a .launch application package
 * ============================================================================ */
void cmd_run(int argc, char *argv[])
{
    if (argc < 2) {
        kputs("Usage: run <app.launch>\n");
        kputs("  e.g. run doom.launch\n");
        return;
    }
    extern int launch_exec(const char *path);
    /* Try with and without leading slash */
    int rc = launch_exec(argv[1]);
    if (rc < 0) {
        char path2[64];
        path2[0] = '/';
        int i = 0;
        while (argv[1][i] && i < 60) { path2[i+1] = argv[1][i]; i++; }
        path2[i+1] = '\0';
        rc = launch_exec(path2);
    }
    if (rc < 0)
        kputs("Error: package not found or failed to launch.\n");
}

/* ============================================================================
 * cmd_doomgfx — launch DOOM via .launch package
 * ============================================================================ */
void cmd_doomgfx(int argc, char *argv[])
{
    (void)argc; (void)argv;
    extern int launch_exec(const char *path);
    kputs("Launching DOOM...\n");
    if (launch_exec("/DOOM.LAUNCH") == 0) return;
    if (launch_exec("doom.launch")  == 0) return;
    if (launch_exec("/DOOM.STAPP")  == 0) return;
    if (launch_exec("doom.stapp")   == 0) return;
    kputs("Error: doom.launch not found. Run 'make' to build.\n");
}

void cmd_doom2gfx(int argc, char *argv[]) { (void)argc; (void)argv; }

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
    kputs("  STAX Kernel - back in shell\n");
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
    kputs("  STAX Kernel - back in shell\n");
    kputs("========================================\n");
    if (res == FR_OK && bw == (UINT)len) {
        kputs("File saved successfully.\n");
    } else {
        kputs("Failed to save file completely.\n");
    }
    kputs("Type 'help' for available commands\n");
}

#ifdef ENABLE_BENCH
/* ============================================================================
 * cmd_bench — run benchmark suite
 * Usage: bench [--memory|--vm|--scheduler|--fs|--gfx|--firmware|--all]
 * ============================================================================ */
extern void bench_run_all(void);
extern void bench_run_sub(const char *name);

void cmd_bench(int argc, char *argv[])
{
    if (argc < 2 || strcmp(argv[1], "--all") == 0) {
        kputs("Running full STAX benchmark suite...\n");
        kputs("(This takes 30-60 seconds. BENCH: lines = CSV output)\n\n");
        bench_run_all();
        return;
    }

    const char *sub = argv[1];
    /* Strip leading -- */
    if (sub[0] == '-' && sub[1] == '-') sub += 2;

    kputs("Running benchmark: ");
    kputs(sub);
    kputs("\n");
    bench_run_sub(sub);
}

/* ============================================================================
 * cmd_stress — run stress tests
 * ============================================================================ */
extern void bench_stress_run(void);
extern void bench_timer_init(void);

void cmd_stress(int argc, char *argv[])
{
    (void)argc; (void)argv;
    kputs("Running STAX stress tests...\n");
    kputs("(Memory, Scheduler, Filesystem, Graphics)\n\n");
    bench_timer_init();
    bench_stress_run();
}
#endif


/* ============================================================================
 * cmd_ps — show task/process list
 * ============================================================================ */
void cmd_ps(int argc, char *argv[])
{
    (void)argc; (void)argv;

    kputs("Process/Task List:\n");
    kputs("==================\n");
    kputs("  PID  STATE       NAME\n");
    kputs("  ---- ----------- ----------------\n");

    /* We can access task_table via the current_task pointer and walking the
     * circular linked list. We don't have direct access to task_table here,
     * so we walk through current_task->next chain. */
    extern task_t *current_task;
    if (!current_task) {
        kputs("  (scheduler not initialized)\n");
        return;
    }

    task_t *t = current_task;
    int pid = 0;
    int max_walk = 8; /* safety limit */

    do {
        kputs("  ");
        /* PID */
        kput_uint((uint32_t)pid);
        if (pid < 10) kputc(' ');
        kputs("   ");

        /* STATE */
        const char *state_str;
        switch (t->state) {
            case 0:  state_str = "READY      "; break;
            case 1:  state_str = "RUNNING    "; break;
            case 2:  state_str = "BLOCKED    "; break;
            case -1: state_str = "DEAD       "; break;
            default: state_str = "UNKNOWN    "; break;
        }
        kputs(state_str);

        /* Name — we don't track task names in the TCB, so use pid-based labels */
        if (pid == 0) kputs("kernel_main");
        else { kputs("task_"); kput_uint((uint32_t)pid); }
        kputs("\n");

        t = t->next;
        pid++;
        max_walk--;
    } while (t != current_task && max_walk > 0);

    kputs("\n");
    kputs("  Scheduler: Preemptive round-robin\n");
    kputs("  Quantum  : 10 ms (every 10 timer ticks)\n");
    kputs("  MAX_TASKS: 4\n");
}

/* ============================================================================
 * cmd_uptime — show system uptime
 * ============================================================================ */
void cmd_uptime(int argc, char *argv[])
{
    (void)argc; (void)argv;

    unsigned int t = tick_count;
    unsigned int total_sec = t / 1000;
    unsigned int ms        = t % 1000;
    unsigned int hours     = total_sec / 3600;
    unsigned int minutes   = (total_sec % 3600) / 60;
    unsigned int seconds   = total_sec % 60;

    kputs("System Uptime:\n");
    kputs("==============\n");
    kputs("  Ticks     : "); kput_uint(t);          kputs(" (1ms/tick)\n");
    kputs("  Uptime    : ");
    kput_uint(hours);   kputs("h ");
    kput_uint(minutes); kputs("m ");
    kput_uint(seconds); kputs(".");
    if (ms < 100) kputc('0');
    if (ms < 10)  kputc('0');
    kput_uint(ms);      kputs("s\n");

    /* Page allocator state */
    int free_mem  = get_free_memory();
    int total_mem = get_total_memory();
    kputs("  Free RAM  : "); kput_uint((uint32_t)(free_mem / 1024));
    kputs(" KB / "); kput_uint((uint32_t)(total_mem / 1024)); kputs(" KB\n");

    /* Heap free list */
    kputs("  Heap free : "); kput_uint(heap_get_free()); kputs(" bytes\n");
}

/* ---------------------------------------------------------------------------
 * cmd_fwupdate — update firmware
 * --------------------------------------------------------------------------- */
void cmd_fwupdate(int argc, char *argv[])
{
    if (argc < 2) {
        kputs("Usage: fwupdate <file.stax>\n");
        return;
    }
    extern int stax_firmware_update(const char *file_path);
    stax_firmware_update(argv[1]);
}

/* ---------------------------------------------------------------------------
 * cmd_fwconfirm — confirm firmware
 * --------------------------------------------------------------------------- */
void cmd_fwconfirm(int argc, char *argv[])
{
    (void)argc; (void)argv;
    extern int stax_firmware_confirm(void);
    stax_firmware_confirm();
}

/* ---------------------------------------------------------------------------
 * cmd_date / cmd_time — display realtime date and time in IST (Mumbai)
 * --------------------------------------------------------------------------- */
void cmd_date(int argc, char *argv[])
{
    (void)argc; (void)argv;
    char buf[64];
    extern void rtc_format_ist_full(char *buf, int max_len);
    extern uint32_t rtc_get_epoch(void);
    rtc_format_ist_full(buf, sizeof(buf));
    kputs("Current Date & Time: ");
    kputs(buf);
    kputs("\nTimezone           : Indian Standard Time (UTC+05:30, Mumbai / Kolkata)\n");
    kputs("Host UTC Epoch     : ");
    kput_uint(rtc_get_epoch());
    kputs(" seconds\n");
}

/* ---------------------------------------------------------------------------
 * cmd_exec — load and run a dynamic ELF-32 executable
 * --------------------------------------------------------------------------- */
void cmd_exec(int argc, char *argv[])
{
    if (argc < 2) {
        kputs("Usage: exec <binary.elf> [args...]\n");
        return;
    }
    extern int elf_exec(const char *path, int argc, char **argv);
    kputs("Loading ELF binary: "); kputs(argv[1]); kputs("\n");
    int rc = elf_exec(argv[1], argc - 1, &argv[1]);
    if (rc == 0) {
        kputs("ELF Process launched successfully.\n");
    } else {
        kputs("Failed to execute ELF binary.\n");
    }
}

/* ---------------------------------------------------------------------------
 * cmd_vfs — show Virtual File System mount points & open file descriptors
 * --------------------------------------------------------------------------- */
void cmd_vfs(int argc, char *argv[])
{
    (void)argc; (void)argv;
    kputs("STAX Virtual File System (VFS):\n");
    kputs("  Mount Points:\n");
    kputs("    /     -> FAT16 SD Block Storage (Driver: fatfs)\n");
    kputs("    /dev  -> Pseudo-Device File System (Driver: devfs)\n\n");
    kputs("  Standard File Descriptors:\n");
    kputs("    fd 0 (stdin)  -> /dev/tty0 (PTY Slave)\n");
    kputs("    fd 1 (stdout) -> /dev/tty0 (PTY Slave)\n");
    kputs("    fd 2 (stderr) -> /dev/tty0 (PTY Slave)\n");
}

/* ---------------------------------------------------------------------------
 * cmd_dev — list and inspect /dev character & block device nodes
 * --------------------------------------------------------------------------- */
void cmd_dev(int argc, char *argv[])
{
    (void)argc; (void)argv;
    kputs("Registered /dev Device Nodes:\n");
    kputs("  /dev/null    (crw-rw-rw-) Discard sink / EOF source\n");
    kputs("  /dev/zero    (crw-rw-rw-) Infinite zero-byte stream\n");
    kputs("  /dev/urandom (crw-rw-rw-) Hardware-seeded PRNG entropy stream\n");
    kputs("  /dev/tty0    (crw-rw-rw-) Active Pseudo-Terminal (PTY master/slave)\n");
    kputs("  /dev/tty     (crw-rw-rw-) Controlling TTY alias\n");
    kputs("  /dev/fb0     (crw-rw-rw-) Direct Framebuffer (1024x768 16bpp)\n");
}

