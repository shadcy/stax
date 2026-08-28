/**
 * @file    pipe.h
 * @author  shadcy
 * @brief   Anonymous Pipe Subsystem for IPC in STAX.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#ifndef STAX_PIPE_H
#define STAX_PIPE_H

#include <stdint.h>
#include <stddef.h>
#include "vfs.h"

#define PIPE_BUF_SIZE   2048

/* Pipe IPC Channel Structure */
typedef struct pipe {
    char        buffer[PIPE_BUF_SIZE];
    uint32_t    head;           /* Write index */
    uint32_t    tail;           /* Read index */
    uint32_t    bytes_avail;
    int         readers_open;
    int         writers_open;
    vfs_node_t  read_node;
    vfs_node_t  write_node;
} pipe_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the pipe allocator pool */
void pipe_init(void);

/* Create an anonymous pipe pair and populate pipefd[0] (read) and pipefd[1] (write) */
int pipe_create(int pipefd[2]);

#ifdef __cplusplus
}
#endif

#endif /* STAX_PIPE_H */
