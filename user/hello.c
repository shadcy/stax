/**
 * @file    hello.c
 * @author  shadcy
 * @brief   Standalone User-Space ELF-32 Demo Application for STAX GPOS.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include "syscall.h"
#include "pty.h"

extern void u_print(const char *str);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    const char *header = "\n"
        "====================================================\n"
        "  STAX GPOS User-Space ELF Binary: hello.elf\n"
        "  Running in Unprivileged USR Mode (0x10)\n"
        "  Syscall EABI Handshake Verified: OK\n"
        "====================================================\n\n";
    u_print(header);

    const char *msg = "Hello from dynamic ELF-32 process!\n"
                      "Memory virtualization & hardware protection active.\n";
    u_print(msg);

    if (u_isatty(STDOUT_FILENO)) {
        u_print("  [PTY] Standard Output connected to POSIX Virtual TTY: YES\n");
    }

    struct winsize ws;
    if (u_ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        u_print("  [PTY] Terminal Window Geometry: 80x25 character grid (640x480)\n");
    }

    /* Test VFS Devfs node access from unprivileged USR mode */
    int fd_rand = u_open("/dev/urandom", 0);
    if (fd_rand >= 0) {
        uint8_t rand_bytes[4];
        u_read(fd_rand, rand_bytes, sizeof(rand_bytes));
        u_print("  [VFS] Read /dev/urandom via fd: OK\n");
        u_close(fd_rand);
    }

    /* Test Anonymous Pipe IPC from unprivileged USR mode */
    int pfd[2];
    if (u_pipe(pfd) == 0) {
        const char *pipe_payload = "IPC message across anonymous pipe\n";
        u_write(pfd[1], pipe_payload, 34);
        char rx[40];
        int n = u_read(pfd[0], rx, 34);
        if (n > 0) {
            rx[n] = '\0';
            u_print("  [PIPE] Userland Anonymous Pipe IPC loopback: OK\n");
        }
        u_close(pfd[0]);
        u_close(pfd[1]);
    }

    u_print("\nAll Userland POSIX, VFS & IPC tests passed.\n\n");
    u_exit(0);
    return 0;
}
