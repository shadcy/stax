/**
 * @file    vfs.h
 * @author  shadcy
 * @brief   Virtual File System (VFS) & File Descriptor Interface for STAX.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#ifndef STAX_VFS_H
#define STAX_VFS_H

#include <stdint.h>
#include <stddef.h>

/* Node Types */
#define VFS_TYPE_FILE       0x01
#define VFS_TYPE_DIR        0x02
#define VFS_TYPE_CHARDEV    0x03
#define VFS_TYPE_BLOCKDEV   0x04
#define VFS_TYPE_PIPE       0x05
#define VFS_TYPE_SYMLINK    0x06

/* POSIX File Access Flags */
#define O_RDONLY            0x0000
#define O_WRONLY            0x0001
#define O_RDWR              0x0002
#define O_CREAT             0x0040
#define O_TRUNC             0x0200
#define O_APPEND            0x0400
#define O_NONBLOCK          0x0800

/* Lseek Whence Constants */
#define SEEK_SET            0
#define SEEK_CUR            1
#define SEEK_END            2

#define MAX_FDS             32
#define VFS_MAX_MOUNTS      8

struct vfs_node;

/* VFS Operations Vector */
typedef struct vfs_ops {
    int     (*open)(struct vfs_node *node, int flags);
    int     (*close)(struct vfs_node *node);
    int32_t (*read)(struct vfs_node *node, uint32_t offset, void *buf, size_t count);
    int32_t (*write)(struct vfs_node *node, uint32_t offset, const void *buf, size_t count);
    int     (*ioctl)(struct vfs_node *node, uint32_t cmd, void *arg);
    int     (*stat)(struct vfs_node *node, void *stat_buf);
} vfs_ops_t;

/* Generic VFS Node / Inode */
typedef struct vfs_node {
    char            name[32];
    uint32_t        type;
    uint32_t        size;
    uint32_t        flags;
    uint32_t        inode;
    vfs_ops_t      *ops;
    void           *priv_data;
    struct vfs_node *next;
    struct vfs_node *children;
} vfs_node_t;

/* File Descriptor Table Entry */
typedef struct {
    vfs_node_t *node;
    uint32_t    offset;
    int         flags;
    int         in_use;
} file_descriptor_t;

/* VFS Mount Point Entry */
typedef struct {
    char        mount_path[32];
    vfs_node_t *root_node;
} vfs_mount_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the unified VFS and allocate standard descriptors */
void vfs_init(void);

/* Mount a filesystem / devfs at a given path prefix */
int vfs_mount(const char *mount_path, vfs_node_t *root_node);

/* Resolve path to a VFS node */
vfs_node_t *vfs_lookup(const char *path);

/* High-level File Descriptor API */
int vfs_open(const char *path, int flags);
int vfs_bind_fd(vfs_node_t *node, int flags);
int vfs_close(int fd);
int32_t vfs_read(int fd, void *buf, size_t count);
int32_t vfs_write(int fd, const void *buf, size_t count);
int32_t vfs_lseek(int fd, int32_t offset, int whence);
int vfs_ioctl(int fd, uint32_t cmd, void *arg);
int vfs_isatty(int fd);

#ifdef __cplusplus
}
#endif

#endif /* STAX_VFS_H */
