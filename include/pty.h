/**
 * @file    pty.h
 * @author  shadcy
 * @brief   POSIX Pseudo-Terminal (PTY) Subsystem & Termios Definitions for STAX.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#ifndef STAX_PTY_H
#define STAX_PTY_H

#include <stdint.h>
#include <stddef.h>

/* Termios Flags (POSIX Compatible) */
#define ECHO            0x0001
#define ECHONL          0x0002
#define ICANON          0x0004
#define ISIG            0x0008
#define ICRNL           0x0010
#define ONLCR           0x0020
#define OPOST           0x0040

/* IOCTL Terminal Commands */
#define TCGETS          0x5401
#define TCSETS          0x5402
#define TIOCGWINSZ      0x5413
#define TIOCSWINSZ      0x5414

#define PTY_MAX_PAIRS   4
#define PTY_BUF_SIZE    1024

/* Window Size Structure */
struct winsize {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
};

/* POSIX Termios Structure */
struct termios {
    uint32_t c_iflag;       /* Input modes */
    uint32_t c_oflag;       /* Output modes */
    uint32_t c_cflag;       /* Control modes */
    uint32_t c_lflag;       /* Local modes */
    uint8_t  c_cc[16];      /* Control characters */
};

/* PTY Circular Buffer */
typedef struct {
    char buffer[PTY_BUF_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} pty_fifo_t;

/* Pseudo-Terminal Pair */
typedef struct pty {
    int id;
    int in_use;
    pty_fifo_t in_buf;      /* Master -> Slave (Input to Process) */
    pty_fifo_t out_buf;     /* Slave -> Master (Output from Process) */
    struct termios termios;
    struct winsize winsize;
    void (*on_output)(struct pty *pty, const char *buf, size_t len);
    void *user_data;
} pty_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize PTY subsystem */
void pty_init(void);

/* Create a new PTY master/slave pair */
pty_t *pty_create(void);

/* Free a PTY pair */
void pty_destroy(pty_t *pty);

/* Master endpoint I/O (GUI window / Emulator side) */
int pty_master_write(pty_t *pty, const char *buf, size_t count);
int pty_master_read(pty_t *pty, char *buf, size_t count);

/* Slave endpoint I/O (Application process side: stdin/stdout) */
int pty_slave_write(pty_t *pty, const char *buf, size_t count);
int pty_slave_read(pty_t *pty, char *buf, size_t count);

/* Active system PTY accessor */
pty_t *pty_get_active(void);
void pty_set_active(pty_t *pty);

#ifdef __cplusplus
}
#endif

#endif /* STAX_PTY_H */
