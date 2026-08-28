/**
 * @file    process.h
 * @author  shadcy
 * @brief   Process Control Block & Lifecycle Management for STAX.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#ifndef STAX_PROCESS_H
#define STAX_PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include "mmu.h"

#define PROC_MAX_PROCESSES  16

#define PROC_STATE_UNUSED   0
#define PROC_STATE_RUNNING  1
#define PROC_STATE_SLEEPING 2
#define PROC_STATE_ZOMBIE   3
#define PROC_STATE_DEAD     4

typedef struct process {
    int         pid;
    int         ppid;
    int         state;
    int         exit_code;
    char        name[32];
    uint32_t    sleep_until;
    pagedir_t  *page_dir;
} process_t;

#ifdef __cplusplus
extern "C" {
#endif

void process_init(void);
process_t *process_create(const char *name);
process_t *process_get(int pid);
process_t *process_get_current(void);
void process_exit(int code);
int process_waitpid(int pid, int *status, int options);
void process_sleep(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* STAX_PROCESS_H */
