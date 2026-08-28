/**
 * @file    syscall.c
 * @author  shadcy
 * @brief   Kernel System Call Dispatcher and Core Syscall Implementations.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include "syscall.h"
#include "console.h"
#include "scheduler.h"
#include "system.h"
#include "timer.h"

extern volatile unsigned int tick_count;

/* ============================================================================
 * Kernel Syscall Handlers
 * ============================================================================ */

static int32_t ksys_write(int fd, const void *buf, size_t count) {
    if (!buf || count == 0) return 0;
    
    /* For stdout and stderr, write directly to kernel console / UART */
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        const char *p = (const char *)buf;
        for (size_t i = 0; i < count; i++) {
            kputc(p[i]);
        }
        return (int32_t)count;
    }
    return -1; /* Unsupported FD for now */
}

static int32_t ksys_read(int fd, void *buf, size_t count) {
    if (!buf || count == 0) return 0;
    
    /* For stdin, read from keyboard buffer / console */
    if (fd == STDIN_FILENO) {
        char *p = (char *)buf;
        size_t read_bytes = 0;
        while (read_bytes < count) {
            int c = kgetc();
            if (c < 0) break;
            p[read_bytes++] = (char)c;
            if (c == '\n' || c == '\r') break;
        }
        return (int32_t)read_bytes;
    }
    return -1;
}

static int32_t ksys_yield(void) {
    /* Request immediate reschedule */
    need_schedule = 1;
    return 0;
}

static int32_t ksys_exit(int code) {
    (void)code;
    kprintf("[SYSCALL] Task exit (PID %d)\n", current_task ? current_task->state : 0);
    task_exit();
    return 0;
}

static int32_t ksys_getpid(void) {
    /* For now, return task ID or index */
    return current_task ? 1 : 0;
}

static int32_t ksys_uptime(void) {
    return (int32_t)tick_count;
}

static int32_t ksys_reboot(void) {
    system_reboot();
    return 0;
}

/* ============================================================================
 * Syscall Dispatcher Table
 * ============================================================================ */

int32_t syscall_dispatch(uint32_t sys_num, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg3;
    switch (sys_num) {
        case SYS_EXIT:
            return ksys_exit((int)arg0);
            
        case SYS_YIELD:
            return ksys_yield();
            
        case SYS_READ:
            return ksys_read((int)arg0, (void *)arg1, (size_t)arg2);
            
        case SYS_WRITE:
            return ksys_write((int)arg0, (const void *)arg1, (size_t)arg2);
            
        case SYS_GETPID:
            return ksys_getpid();
            
        case SYS_UPTIME:
            return ksys_uptime();
            
        case SYS_REBOOT:
            return ksys_reboot();
            
        default:
            kprintf("[SYSCALL] Unknown syscall #%u (args: 0x%x, 0x%x, 0x%x)\n", sys_num, arg0, arg1, arg2);
            return -1;
    }
}
