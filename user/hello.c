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
                      "Memory virtualization & hardware protection active.\n\n";
    u_print(msg);

    u_exit(0);
    return 0;
}
