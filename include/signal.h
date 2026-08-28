/**
 * @file    signal.h
 * @author  shadcy
 * @brief   POSIX Signal Handling Subsystem Definitions for STAX.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#ifndef STAX_SIGNAL_H
#define STAX_SIGNAL_H

#include <stdint.h>
#include <stddef.h>

/* POSIX Standard Signals */
#define SIGHUP          1
#define SIGINT          2       /* Interactive Attention / Ctrl+C */
#define SIGQUIT         3
#define SIGILL          4       /* Illegal Instruction */
#define SIGTRAP         5
#define SIGABRT         6       /* Abnormal Termination */
#define SIGBUS          7
#define SIGFPE          8       /* Floating-Point Exception */
#define SIGKILL         9       /* Uncatchable Kill */
#define SIGUSR1         10
#define SIGSEGV         11      /* Segmentation Fault / Memory Protection Violation */
#define SIGUSR2         12
#define SIGPIPE         13      /* Broken Pipe */
#define SIGALRM         14      /* Alarm Clock */
#define SIGTERM         15      /* Termination Request */
#define SIGCHLD         17      /* Child Process Terminated or Stopped */
#define SIGCONT         18      /* Continue if Stopped */
#define SIGSTOP         19      /* Uncatchable Stop */
#define SIGTSTP         20      /* Keyboard Stop */

#define NSIG            32

typedef void (*sighandler_t)(int);

#define SIG_DFL         ((sighandler_t)0)
#define SIG_IGN         ((sighandler_t)1)
#define SIG_ERR         ((sighandler_t)-1)

/* Signal Action Structure */
struct sigaction {
    sighandler_t sa_handler;
    uint32_t     sa_mask;
    int          sa_flags;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the kernel signal subsystem */
void signal_init(void);

/* Register signal handler for current task/process */
sighandler_t signal_register(int signum, sighandler_t handler);

/* Send signal to a target process */
int signal_send(int pid, int signum);

/* Intercept hardware aborts and convert to user signal */
void signal_raise_fault(int signum, const char *reason);

#ifdef __cplusplus
}
#endif

#endif /* STAX_SIGNAL_H */
