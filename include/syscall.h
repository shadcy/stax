/**
 * @file    syscall.h
 * @author  shadcy
 * @brief   System Call Interface and Syscall Numbers for STAX GPOS.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * Standard Syscall Numbers (ARM EABI Convention)
 * ============================================================================ */
#define SYS_EXIT        1
#define SYS_YIELD       2
#define SYS_READ        3
#define SYS_WRITE       4
#define SYS_OPEN        5
#define SYS_CLOSE       6
#define SYS_EXECVE      11
#define SYS_GETPID      20
#define SYS_UPTIME      25
#define SYS_REBOOT      88

/* Standard file descriptors */
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

#ifdef __cplusplus
extern "C" {
#endif

/* Kernel-side dispatcher entry point */
int32_t syscall_dispatch(uint32_t sys_num, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

/* ============================================================================
 * User-space Inline Syscall Invocation Helpers (ARM EABI in USR mode)
 *
 * Syscall number passed in r7, arguments in r0-r3, return value in r0.
 * ============================================================================ */

static inline int32_t _syscall0(uint32_t num) {
    register uint32_t r7 __asm__("r7") = num;
    register int32_t  r0 __asm__("r0");
    __asm__ volatile (
        "svc #0\n"
        : "=r"(r0)
        : "r"(r7)
        : "memory", "cc"
    );
    return r0;
}

static inline int32_t _syscall1(uint32_t num, uint32_t arg0) {
    register uint32_t r7 __asm__("r7") = num;
    register int32_t  r0 __asm__("r0") = (int32_t)arg0;
    __asm__ volatile (
        "svc #0\n"
        : "=r"(r0)
        : "r"(r7), "0"(r0)
        : "memory", "cc"
    );
    return r0;
}

static inline int32_t _syscall2(uint32_t num, uint32_t arg0, uint32_t arg1) {
    register uint32_t r7 __asm__("r7") = num;
    register int32_t  r0 __asm__("r0") = (int32_t)arg0;
    register uint32_t r1 __asm__("r1") = arg1;
    __asm__ volatile (
        "svc #0\n"
        : "=r"(r0)
        : "r"(r7), "0"(r0), "r"(r1)
        : "memory", "cc"
    );
    return r0;
}

static inline int32_t _syscall3(uint32_t num, uint32_t arg0, uint32_t arg1, uint32_t arg2) {
    register uint32_t r7 __asm__("r7") = num;
    register int32_t  r0 __asm__("r0") = (int32_t)arg0;
    register uint32_t r1 __asm__("r1") = arg1;
    register uint32_t r2 __asm__("r2") = arg2;
    __asm__ volatile (
        "svc #0\n"
        : "=r"(r0)
        : "r"(r7), "0"(r0), "r"(r1), "r"(r2)
        : "memory", "cc"
    );
    return r0;
}

/* User-space Convenience Wrappers */
static inline void u_exit(int code) {
    _syscall1(SYS_EXIT, (uint32_t)code);
    while (1);
}

static inline void u_yield(void) {
    _syscall0(SYS_YIELD);
}

static inline int32_t u_write(int fd, const void *buf, size_t count) {
    return _syscall3(SYS_WRITE, (uint32_t)fd, (uint32_t)buf, (uint32_t)count);
}

static inline int32_t u_read(int fd, void *buf, size_t count) {
    return _syscall3(SYS_READ, (uint32_t)fd, (uint32_t)buf, (uint32_t)count);
}

static inline int32_t u_getpid(void) {
    return _syscall0(SYS_GETPID);
}

static inline uint32_t u_uptime(void) {
    return (uint32_t)_syscall0(SYS_UPTIME);
}

static inline int32_t u_execve(const char *path, char *const argv[], char *const envp[]) {
    return _syscall3(SYS_EXECVE, (uint32_t)path, (uint32_t)argv, (uint32_t)envp);
}

#ifdef __cplusplus
}
#endif

#endif /* SYSCALL_H */
