/**
 * @file    process.c
 * @author  shadcy
 * @brief   Process Control Block & Lifecycle Management for STAX.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include "process.h"
#include "string.h"
#include "console.h"

extern volatile unsigned int tick_count;

static process_t g_proc_table[PROC_MAX_PROCESSES];
static int       g_current_pid = 1;

void process_init(void) {
    memset(g_proc_table, 0, sizeof(g_proc_table));

    /* Process 1: Kernel Root Process */
    g_proc_table[0].pid   = 1;
    g_proc_table[0].ppid  = 0;
    g_proc_table[0].state = PROC_STATE_RUNNING;
    strcpy(g_proc_table[0].name, "system");
    g_current_pid = 1;

    kputs("PROCESS: Process Control Subsystem initialized (PID 1 active).\n");
}

process_t *process_create(const char *name) {
    for (int i = 0; i < PROC_MAX_PROCESSES; i++) {
        if (g_proc_table[i].state == PROC_STATE_UNUSED || g_proc_table[i].state == PROC_STATE_DEAD) {
            process_t *proc = &g_proc_table[i];
            proc->pid       = i + 1;
            proc->ppid      = g_current_pid;
            proc->state     = PROC_STATE_RUNNING;
            proc->exit_code = 0;
            proc->sleep_until = 0;
            proc->page_dir  = NULL;
            strncpy(proc->name, name ? name : "process", sizeof(proc->name) - 1);
            return proc;
        }
    }
    return NULL;
}

process_t *process_get(int pid) {
    for (int i = 0; i < PROC_MAX_PROCESSES; i++) {
        if (g_proc_table[i].pid == pid && g_proc_table[i].state != PROC_STATE_UNUSED) {
            return &g_proc_table[i];
        }
    }
    return NULL;
}

process_t *process_get_current(void) {
    return process_get(g_current_pid);
}

void process_exit(int code) {
    process_t *cur = process_get_current();
    if (cur) {
        cur->state     = PROC_STATE_ZOMBIE;
        cur->exit_code = code;
    }
}

int process_waitpid(int pid, int *status, int options) {
    (void)options;
    process_t *child = process_get(pid);
    if (!child) return -1;

    if (child->state == PROC_STATE_ZOMBIE) {
        if (status) *status = child->exit_code;
        child->state = PROC_STATE_DEAD;
        return pid;
    }
    return 0;
}

void process_sleep(uint32_t ms) {
    uint32_t target = tick_count + ms;
    while (tick_count < target) {
        asm volatile("nop");
    }
}
