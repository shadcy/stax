/**
 * @file    pipe.c
 * @author  shadcy
 * @brief   Anonymous Pipe IPC Channel & VFS Integration for STAX.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include "pipe.h"
#include "heap.h"
#include "string.h"
#include "console.h"

#define MAX_PIPES 16
static pipe_t g_pipe_pool[MAX_PIPES];
static int    g_pipe_inited = 0;

/* ============================================================================
 * Pipe Read Operations
 * ============================================================================ */

static int32_t pipe_vfs_read(vfs_node_t *node, uint32_t offset, void *buf, size_t count) {
    (void)offset;
    if (!node || !node->priv_data || !buf || count == 0) return 0;
    pipe_t *p = (pipe_t *)node->priv_data;

    if (p->bytes_avail == 0) {
        if (p->writers_open == 0) {
            return 0; /* All writers closed -> EOF */
        }
        return 0; /* Non-blocking / empty return */
    }

    size_t to_read = count;
    if (to_read > p->bytes_avail) {
        to_read = p->bytes_avail;
    }

    char *dst = (char *)buf;
    for (size_t i = 0; i < to_read; i++) {
        dst[i] = p->buffer[p->tail];
        p->tail = (p->tail + 1) % PIPE_BUF_SIZE;
    }
    p->bytes_avail -= to_read;
    return (int32_t)to_read;
}

static int pipe_vfs_read_close(vfs_node_t *node) {
    if (!node || !node->priv_data) return -1;
    pipe_t *p = (pipe_t *)node->priv_data;
    if (p->readers_open > 0) p->readers_open--;
    return 0;
}

static vfs_ops_t g_pipe_read_ops = {
    .open  = NULL,
    .close = pipe_vfs_read_close,
    .read  = pipe_vfs_read,
    .write = NULL,
    .ioctl = NULL,
    .stat  = NULL
};

/* ============================================================================
 * Pipe Write Operations
 * ============================================================================ */

static int32_t pipe_vfs_write(vfs_node_t *node, uint32_t offset, const void *buf, size_t count) {
    (void)offset;
    if (!node || !node->priv_data || !buf || count == 0) return 0;
    pipe_t *p = (pipe_t *)node->priv_data;

    /* If all readers closed -> Broken pipe (EPIPE) */
    if (p->readers_open == 0) {
        return -1;
    }

    size_t space_avail = PIPE_BUF_SIZE - p->bytes_avail;
    size_t to_write = count;
    if (to_write > space_avail) {
        to_write = space_avail;
    }
    if (to_write == 0) return 0;

    const char *src = (const char *)buf;
    for (size_t i = 0; i < to_write; i++) {
        p->buffer[p->head] = src[i];
        p->head = (p->head + 1) % PIPE_BUF_SIZE;
    }
    p->bytes_avail += to_write;
    return (int32_t)to_write;
}

static int pipe_vfs_write_close(vfs_node_t *node) {
    if (!node || !node->priv_data) return -1;
    pipe_t *p = (pipe_t *)node->priv_data;
    if (p->writers_open > 0) p->writers_open--;
    return 0;
}

static vfs_ops_t g_pipe_write_ops = {
    .open  = NULL,
    .close = pipe_vfs_write_close,
    .read  = NULL,
    .write = pipe_vfs_write,
    .ioctl = NULL,
    .stat  = NULL
};

/* ============================================================================
 * Pipe Lifecycle & Creation
 * ============================================================================ */

void pipe_init(void) {
    if (g_pipe_inited) return;
    memset(g_pipe_pool, 0, sizeof(g_pipe_pool));
    g_pipe_inited = 1;
}

int pipe_create(int pipefd[2]) {
    if (!pipefd) return -1;
    if (!g_pipe_inited) pipe_init();

    /* Find an available pipe channel */
    pipe_t *p = NULL;
    for (int i = 0; i < MAX_PIPES; i++) {
        if (g_pipe_pool[i].readers_open == 0 && g_pipe_pool[i].writers_open == 0) {
            p = &g_pipe_pool[i];
            break;
        }
    }
    if (!p) return -1;

    memset(p, 0, sizeof(pipe_t));
    p->readers_open = 1;
    p->writers_open = 1;

    /* Initialize Read Node */
    strcpy(p->read_node.name, "pipe:[read]");
    p->read_node.type      = VFS_TYPE_PIPE;
    p->read_node.ops       = &g_pipe_read_ops;
    p->read_node.priv_data = p;

    /* Initialize Write Node */
    strcpy(p->write_node.name, "pipe:[write]");
    p->write_node.type      = VFS_TYPE_PIPE;
    p->write_node.ops       = &g_pipe_write_ops;
    p->write_node.priv_data = p;

    /* Bind to Process File Descriptor Table */
    int rfd = vfs_bind_fd(&p->read_node, O_RDONLY);
    if (rfd < 0) {
        p->readers_open = 0;
        p->writers_open = 0;
        return -1;
    }

    int wfd = vfs_bind_fd(&p->write_node, O_WRONLY);
    if (wfd < 0) {
        vfs_close(rfd);
        p->readers_open = 0;
        p->writers_open = 0;
        return -1;
    }

    pipefd[0] = rfd;
    pipefd[1] = wfd;
    return 0;
}
