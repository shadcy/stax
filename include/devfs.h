/**
 * @file    devfs.h
 * @author  shadcy
 * @brief   Device Filesystem (/dev) for STAX.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#ifndef STAX_DEVFS_H
#define STAX_DEVFS_H

#include "vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize devfs and register standard devices (/dev/null, /dev/zero, /dev/urandom, /dev/tty0, /dev/fb0) */
void devfs_init(void);

/* Retrieve root node of devfs for mounting at /dev */
vfs_node_t *devfs_get_root(void);

/* Register a custom device node under /dev */
int devfs_register_device(const char *name, uint32_t type, vfs_ops_t *ops, void *priv_data);

#ifdef __cplusplus
}
#endif

#endif /* STAX_DEVFS_H */
