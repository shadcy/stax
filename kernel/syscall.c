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

#include "pty.h"
#include "vfs.h"
#include "pipe.h"
#include "signal.h"
#include "process.h"
#include "string.h"

/* ============================================================================
 * Kernel Syscall Handlers (VFS & POSIX Backed)
 * ============================================================================ */

static int32_t ksys_open(const char *path, int flags) {
    return vfs_open(path, flags);
}

static int32_t ksys_close(int fd) {
    return vfs_close(fd);
}

static int32_t ksys_read(int fd, void *buf, size_t count) {
    return vfs_read(fd, buf, count);
}

static int32_t ksys_write(int fd, const void *buf, size_t count) {
    return vfs_write(fd, buf, count);
}

static int32_t ksys_lseek(int fd, int32_t offset, int whence) {
    return vfs_lseek(fd, offset, whence);
}

static int32_t ksys_pipe(int pipefd[2]) {
    return pipe_create(pipefd);
}

static int32_t ksys_signal(int signum, sighandler_t handler) {
    sighandler_t old = signal_register(signum, handler);
    return (old == SIG_ERR) ? -1 : 0;
}

static int32_t ksys_kill(int pid, int signum) {
    return signal_send(pid, signum);
}

static int32_t ksys_waitpid(int pid, int *status, int options) {
    return process_waitpid(pid, status, options);
}

static int32_t ksys_sleep(uint32_t ms) {
    process_sleep(ms);
    return 0;
}

static int32_t ksys_ioctl(int fd, uint32_t cmd, void *arg) {
    return vfs_ioctl(fd, cmd, arg);
}

static int32_t ksys_isatty(int fd) {
    return vfs_isatty(fd);
}

static int32_t ksys_yield(void) {
    /* Request immediate reschedule */
    need_schedule = 1;
    return 0;
}

static int32_t ksys_exit(int code) {
    kprintf("[SYSCALL] Task exit (status code: %d)\n", code);
    process_exit(code);
    return (int32_t)code;
}

static int32_t ksys_getpid(void) {
    process_t *cur = process_get_current();
    return cur ? cur->pid : 1;
}

static int32_t ksys_uptime(void) {
    return (int32_t)tick_count;
}

static int32_t ksys_reboot(void) {
    system_reboot();
    return 0;
}

static int32_t ksys_execve(const char *path, char *const argv[], char *const envp[]) {
    (void)envp;
    extern int elf_exec(const char *path, int argc, char **argv);
    int argc = 0;
    if (argv) {
        while (argv[argc]) argc++;
    }
    return elf_exec(path, argc, (char **)argv);
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

        case SYS_OPEN:
            return ksys_open((const char *)arg0, (int)arg1);

        case SYS_CLOSE:
            return ksys_close((int)arg0);

        case SYS_WAITPID:
            return ksys_waitpid((int)arg0, (int *)arg1, (int)arg2);
            
        case SYS_READ:
            return ksys_read((int)arg0, (void *)arg1, (size_t)arg2);
            
        case SYS_WRITE:
            return ksys_write((int)arg0, (const void *)arg1, (size_t)arg2);

        case SYS_LSEEK:
            return ksys_lseek((int)arg0, (int32_t)arg1, (int)arg2);

        case SYS_EXECVE:
            return ksys_execve((const char *)arg0, (char *const *)arg1, (char *const *)arg2);

        case SYS_KILL:
            return ksys_kill((int)arg0, (int)arg1);

        case SYS_PIPE:
            return ksys_pipe((int *)arg0);

        case SYS_SIGNAL:
            return ksys_signal((int)arg0, (sighandler_t)arg1);
            
        case SYS_IOCTL:
            return ksys_ioctl((int)arg0, (uint32_t)arg1, (void *)arg2);

        case SYS_ISATTY:
            return ksys_isatty((int)arg0);

        case SYS_GETPID:
            return ksys_getpid();
            
        case SYS_UPTIME:
            return ksys_uptime();

        case SYS_SLEEP:
            return ksys_sleep((uint32_t)arg0);
            
        case SYS_REBOOT:
            return ksys_reboot();
            
        default:
            kprintf("[SYSCALL] Unknown syscall #%u (args: 0x%x, 0x%x, 0x%x)\n", sys_num, arg0, arg1, arg2);
            return -1;
    }
}
