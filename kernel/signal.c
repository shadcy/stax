/**
 * @file    signal.c
 * @author  shadcy
 * @brief   POSIX Signal Handling Subsystem & Fault Dispatcher for STAX.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include "signal.h"
#include "console.h"
#include "string.h"

static sighandler_t g_signal_handlers[NSIG];

void signal_init(void) {
    for (int i = 0; i < NSIG; i++) {
        g_signal_handlers[i] = SIG_DFL;
    }
    kputs("SIGNAL: POSIX Signal Handling Subsystem initialized.\n");
}

sighandler_t signal_register(int signum, sighandler_t handler) {
    if (signum <= 0 || signum >= NSIG) return SIG_ERR;
    if (signum == SIGKILL || signum == SIGSTOP) return SIG_ERR; /* Uncatchable */

    sighandler_t old = g_signal_handlers[signum];
    g_signal_handlers[signum] = handler;
    return old;
}

int signal_send(int pid, int signum) {
    (void)pid;
    if (signum <= 0 || signum >= NSIG) return -1;

    sighandler_t handler = g_signal_handlers[signum];

    if (handler == SIG_IGN) {
        return 0; /* Ignored */
    }

    if (handler != SIG_DFL && handler != SIG_IGN && handler != SIG_ERR) {
        /* Invoke registered user signal handler */
        handler(signum);
        return 0;
    }

    /* Default Signal Actions */
    switch (signum) {
        case SIGINT:
            kputs("\n[SIGNAL] Interrupt (SIGINT received - Ctrl+C)\n");
            break;
        case SIGSEGV:
            kputs("\n[SIGNAL] Segmentation fault (SIGSEGV - invalid memory access)\n");
            break;
        case SIGTERM:
            kputs("\n[SIGNAL] Terminated (SIGTERM)\n");
            break;
        case SIGKILL:
            kputs("\n[SIGNAL] Killed (SIGKILL)\n");
            break;
        case SIGPIPE:
            kputs("\n[SIGNAL] Broken pipe (SIGPIPE)\n");
            break;
        case SIGALRM:
            kputs("\n[SIGNAL] Alarm clock (SIGALRM)\n");
            break;
        case SIGCHLD:
        case SIGCONT:
            break; /* Ignored by default */
        default:
            kprintf("\n[SIGNAL] Process received signal %d\n", signum);
            break;
    }
    return 0;
}

void signal_raise_fault(int signum, const char *reason) {
    if (reason) {
        kprintf("[FAULT] %s (Signal: %d)\n", reason, signum);
    }
    signal_send(0, signum);
}
