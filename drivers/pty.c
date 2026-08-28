/**
 * @file    pty.c
 * @author  shadcy
 * @brief   POSIX Pseudo-Terminal (PTY) Implementation & Line Discipline for STAX.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include "pty.h"
#include "string.h"
#include "console.h"

static pty_t g_pty_pool[PTY_MAX_PAIRS];
static pty_t *g_active_pty = NULL;

/* ============================================================================
 * Internal FIFO Buffer Operations
 * ============================================================================ */

static inline int fifo_is_full(const pty_fifo_t *f) {
    return ((f->head + 1) % PTY_BUF_SIZE) == f->tail;
}

static inline int fifo_is_empty(const pty_fifo_t *f) {
    return f->head == f->tail;
}

static int fifo_push(pty_fifo_t *f, char c) {
    if (fifo_is_full(f)) return 0;
    f->buffer[f->head] = c;
    f->head = (f->head + 1) % PTY_BUF_SIZE;
    return 1;
}

static int fifo_pop(pty_fifo_t *f, char *c) {
    if (fifo_is_empty(f)) return 0;
    *c = f->buffer[f->tail];
    f->tail = (f->tail + 1) % PTY_BUF_SIZE;
    return 1;
}

/* ============================================================================
 * PTY Lifecycle & Allocation
 * ============================================================================ */

void pty_init(void) {
    memset(g_pty_pool, 0, sizeof(g_pty_pool));
    for (int i = 0; i < PTY_MAX_PAIRS; i++) {
        g_pty_pool[i].id = i;
        g_pty_pool[i].in_use = 0;
    }
    
    /* Create default system console PTY */
    g_active_pty = pty_create();
    kputs("PTY: Pseudo-terminal subsystem initialized (4 pairs supported).\n");
}

pty_t *pty_create(void) {
    for (int i = 0; i < PTY_MAX_PAIRS; i++) {
        if (!g_pty_pool[i].in_use) {
            pty_t *p = &g_pty_pool[i];
            p->in_use = 1;
            p->in_buf.head = p->in_buf.tail = 0;
            p->out_buf.head = p->out_buf.tail = 0;
            p->on_output = NULL;
            p->user_data = NULL;

            /* Default POSIX terminal attributes */
            p->termios.c_iflag = ICRNL;
            p->termios.c_oflag = OPOST | ONLCR;
            p->termios.c_lflag = ECHO | ICANON | ISIG;
            p->termios.c_cflag = 0;

            /* Default 80x25 character grid */
            p->winsize.ws_row = 25;
            p->winsize.ws_col = 80;
            p->winsize.ws_xpixel = 640;
            p->winsize.ws_ypixel = 480;

            if (!g_active_pty) g_active_pty = p;
            return p;
        }
    }
    return NULL;
}

void pty_destroy(pty_t *pty) {
    if (!pty || !pty->in_use) return;
    pty->in_use = 0;
    if (g_active_pty == pty) {
        g_active_pty = NULL;
        for (int i = 0; i < PTY_MAX_PAIRS; i++) {
            if (g_pty_pool[i].in_use) {
                g_active_pty = &g_pty_pool[i];
                break;
            }
        }
    }
}

pty_t *pty_get_active(void) {
    return g_active_pty;
}

void pty_set_active(pty_t *pty) {
    if (pty && pty->in_use) {
        g_active_pty = pty;
    }
}

/* ============================================================================
 * Master Endpoint Operations (Window / Terminal Emulator)
 * ============================================================================ */

int pty_master_write(pty_t *pty, const char *buf, size_t count) {
    if (!pty || !pty->in_use || !buf) return -1;
    size_t written = 0;

    for (size_t i = 0; i < count; i++) {
        char c = buf[i];
        
        /* Input translation: CR -> NL */
        if ((pty->termios.c_iflag & ICRNL) && c == '\r') {
            c = '\n';
        }

        /* Echo character if enabled */
        if (pty->termios.c_lflag & ECHO) {
            fifo_push(&pty->out_buf, c);
            if (pty->on_output) {
                pty->on_output(pty, &c, 1);
            }
        }

        if (fifo_push(&pty->in_buf, c)) {
            written++;
        } else {
            break; /* Buffer full */
        }
    }
    return (int)written;
}

int pty_master_read(pty_t *pty, char *buf, size_t count) {
    if (!pty || !pty->in_use || !buf) return -1;
    size_t read_bytes = 0;

    while (read_bytes < count) {
        char c;
        if (!fifo_pop(&pty->out_buf, &c)) break;
        buf[read_bytes++] = c;
    }
    return (int)read_bytes;
}

/* ============================================================================
 * Slave Endpoint Operations (Process Stdin / Stdout)
 * ============================================================================ */

int pty_slave_write(pty_t *pty, const char *buf, size_t count) {
    if (!pty || !pty->in_use || !buf) return -1;
    size_t written = 0;

    for (size_t i = 0; i < count; i++) {
        char c = buf[i];
        
        /* Output post-processing: NL -> CR NL */
        if ((pty->termios.c_oflag & ONLCR) && c == '\n') {
            fifo_push(&pty->out_buf, '\r');
        }

        if (fifo_push(&pty->out_buf, c)) {
            written++;
        } else {
            break;
        }
    }

    /* Trigger UI callback for live terminal refresh */
    if (pty->on_output && written > 0) {
        pty->on_output(pty, buf, written);
    }
    return (int)written;
}

int pty_slave_read(pty_t *pty, char *buf, size_t count) {
    if (!pty || !pty->in_use || !buf) return -1;
    size_t read_bytes = 0;

    while (read_bytes < count) {
        char c;
        if (!fifo_pop(&pty->in_buf, &c)) break;
        buf[read_bytes++] = c;
        if ((pty->termios.c_lflag & ICANON) && c == '\n') break;
    }
    return (int)read_bytes;
}
