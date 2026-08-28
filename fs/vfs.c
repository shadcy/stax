/**
 * @file    vfs.c
 * @author  shadcy
 * @brief   Virtual File System (VFS) Core & File Descriptor Table Implementation.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include "vfs.h"
#include "syscall.h"
#include "fatfs/ff.h"
#include "heap.h"
#include "string.h"
#include "console.h"
#include "pty.h"

static vfs_mount_t       g_mounts[VFS_MAX_MOUNTS];
static file_descriptor_t g_fd_table[MAX_FDS];

/* ============================================================================
 * FAT16 Adapter Operations for Regular Files
 * ============================================================================ */

typedef struct {
    FIL fil;
} fat_vfs_priv_t;

static int fat_vfs_open(vfs_node_t *node, int flags) {
    fat_vfs_priv_t *priv = (fat_vfs_priv_t *)kmalloc(sizeof(fat_vfs_priv_t));
    if (!priv) return -1;

    BYTE mode = FA_READ;
    if ((flags & O_RDWR) || (flags & O_WRONLY)) {
        mode |= FA_WRITE;
        if (flags & O_CREAT) mode |= FA_OPEN_ALWAYS;
    }

    FRESULT fr = f_open(&priv->fil, node->name, mode);
    if (fr != FR_OK) {
        kfree(priv);
        return -1;
    }
    node->priv_data = priv;
    node->size = f_size(&priv->fil);
    return 0;
}

static int fat_vfs_close(vfs_node_t *node) {
    if (node && node->priv_data) {
        fat_vfs_priv_t *priv = (fat_vfs_priv_t *)node->priv_data;
        f_close(&priv->fil);
        kfree(priv);
        node->priv_data = NULL;
    }
    return 0;
}

static int32_t fat_vfs_read(vfs_node_t *node, uint32_t offset, void *buf, size_t count) {
    if (!node || !node->priv_data || !buf) return -1;
    fat_vfs_priv_t *priv = (fat_vfs_priv_t *)node->priv_data;

    f_lseek(&priv->fil, offset);
    UINT br = 0;
    FRESULT fr = f_read(&priv->fil, buf, count, &br);
    if (fr != FR_OK) return -1;
    return (int32_t)br;
}

static int32_t fat_vfs_write(vfs_node_t *node, uint32_t offset, const void *buf, size_t count) {
    if (!node || !node->priv_data || !buf) return -1;
    fat_vfs_priv_t *priv = (fat_vfs_priv_t *)node->priv_data;

    f_lseek(&priv->fil, offset);
    UINT bw = 0;
    FRESULT fr = f_write(&priv->fil, buf, count, &bw);
    if (fr != FR_OK) return -1;
    node->size = f_size(&priv->fil);
    return (int32_t)bw;
}

static vfs_ops_t g_fat_vfs_ops = {
    .open  = fat_vfs_open,
    .close = fat_vfs_close,
    .read  = fat_vfs_read,
    .write = fat_vfs_write,
    .ioctl = NULL,
    .stat  = NULL
};

/* ============================================================================
 * VFS Initialization & Mount Points
 * ============================================================================ */

void vfs_init(void) {
    memset(g_mounts, 0, sizeof(g_mounts));
    memset(g_fd_table, 0, sizeof(g_fd_table));

    /* Reserve stdin (0), stdout (1), stderr (2) */
    for (int i = 0; i < 3; i++) {
        g_fd_table[i].in_use = 1;
        g_fd_table[i].flags  = (i == 0) ? O_RDONLY : O_WRONLY;
        g_fd_table[i].offset = 0;
        g_fd_table[i].node   = NULL; /* Direct PTY/console routing */
    }

    kputs("VFS: Virtual File System initialized (32 file descriptors available).\n");
}

int vfs_mount(const char *mount_path, vfs_node_t *root_node) {
    if (!mount_path || !root_node) return -1;

    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (g_mounts[i].mount_path[0] == '\0') {
            strncpy(g_mounts[i].mount_path, mount_path, sizeof(g_mounts[i].mount_path) - 1);
            g_mounts[i].root_node = root_node;
            kprintf("VFS: Mounted filesystem at %s\n", mount_path);
            return 0;
        }
    }
    return -1;
}

/* ============================================================================
 * Path Resolution & Lookup
 * ============================================================================ */

vfs_node_t *vfs_lookup(const char *path) {
    if (!path || path[0] == '\0') return NULL;

    /* Check devfs mount: /dev/... */
    if (strncmp(path, "/dev/", 5) == 0 || strncmp(path, "dev/", 4) == 0) {
        const char *dev_name = (path[0] == '/') ? (path + 5) : (path + 4);
        for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
            if (strcmp(g_mounts[i].mount_path, "/dev") == 0 && g_mounts[i].root_node) {
                vfs_node_t *curr = g_mounts[i].root_node->children;
                while (curr) {
                    if (strcmp(curr->name, dev_name) == 0) {
                        return curr;
                    }
                    curr = curr->next;
                }
            }
        }
        return NULL;
    }

    /* Fallback to regular file node on FAT storage */
    vfs_node_t *node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    if (!node) return NULL;
    memset(node, 0, sizeof(vfs_node_t));
    strncpy(node->name, path, sizeof(node->name) - 1);
    node->type = VFS_TYPE_FILE;
    node->ops  = &g_fat_vfs_ops;
    return node;
}

/* ============================================================================
 * High-Level File Descriptor API
 * ============================================================================ */

int vfs_open(const char *path, int flags) {
    if (!path) return -1;

    vfs_node_t *node = vfs_lookup(path);
    if (!node) return -1;

    /* Find free file descriptor slot */
    int fd = -1;
    for (int i = 3; i < MAX_FDS; i++) {
        if (!g_fd_table[i].in_use) {
            fd = i;
            break;
        }
    }
    if (fd < 0) return -1;

    if (node->ops && node->ops->open) {
        if (node->ops->open(node, flags) != 0) {
            if (node->type == VFS_TYPE_FILE) kfree(node);
            return -1;
        }
    }

    g_fd_table[fd].in_use = 1;
    g_fd_table[fd].node   = node;
    g_fd_table[fd].offset = 0;
    g_fd_table[fd].flags  = flags;
    return fd;
}

int vfs_bind_fd(vfs_node_t *node, int flags) {
    if (!node) return -1;
    for (int i = 3; i < MAX_FDS; i++) {
        if (!g_fd_table[i].in_use) {
            g_fd_table[i].in_use = 1;
            g_fd_table[i].node   = node;
            g_fd_table[i].offset = 0;
            g_fd_table[i].flags  = flags;
            return i;
        }
    }
    return -1;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_FDS || !g_fd_table[fd].in_use) return -1;

    vfs_node_t *node = g_fd_table[fd].node;
    if (node && node->ops && node->ops->close) {
        node->ops->close(node);
    }
    if (node && node->type == VFS_TYPE_FILE) {
        kfree(node);
    }

    g_fd_table[fd].in_use = 0;
    g_fd_table[fd].node   = NULL;
    g_fd_table[fd].offset = 0;
    return 0;
}

int32_t vfs_read(int fd, void *buf, size_t count) {
    if (fd < 0 || fd >= MAX_FDS || !g_fd_table[fd].in_use || !buf) return -1;

    /* Standard In */
    if (fd == STDIN_FILENO) {
        pty_t *p = pty_get_active();
        if (p) {
            int n = pty_slave_read(p, (char *)buf, count);
            if (n > 0) return (int32_t)n;
        }
        #define UART0_BASE_V  0x101f1000UL
        #define UART_DR_V     (*(volatile unsigned int *)(UART0_BASE_V + 0x000))
        #define UART_FR_V     (*(volatile unsigned int *)(UART0_BASE_V + 0x018))
        #define UART_RXFE_V   (1 << 4)
        if (!(UART_FR_V & UART_RXFE_V)) {
            ((char *)buf)[0] = (char)(UART_DR_V & 0xFF);
            return 1;
        }
        extern char kb_getc(void);
        char k = kb_getc();
        if (k) {
            ((char *)buf)[0] = k;
            return 1;
        }
        return 0;
    }

    vfs_node_t *node = g_fd_table[fd].node;
    if (!node || !node->ops || !node->ops->read) return -1;

    int32_t res = node->ops->read(node, g_fd_table[fd].offset, buf, count);
    if (res > 0) {
        g_fd_table[fd].offset += res;
    }
    return res;
}

int32_t vfs_write(int fd, const void *buf, size_t count) {
    if (fd < 0 || fd >= MAX_FDS || !g_fd_table[fd].in_use || !buf) return -1;

    /* Standard Out / Err */
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        pty_t *p = pty_get_active();
        if (p) {
            pty_slave_write(p, (const char *)buf, count);
        }
        const char *s = (const char *)buf;
        for (size_t i = 0; i < count; i++) {
            kputc(s[i]);
        }
        return (int32_t)count;
    }

    vfs_node_t *node = g_fd_table[fd].node;
    if (!node || !node->ops || !node->ops->write) return -1;

    int32_t res = node->ops->write(node, g_fd_table[fd].offset, buf, count);
    if (res > 0) {
        g_fd_table[fd].offset += res;
    }
    return res;
}

int32_t vfs_lseek(int fd, int32_t offset, int whence) {
    if (fd < 0 || fd >= MAX_FDS || !g_fd_table[fd].in_use) return -1;

    uint32_t new_off = g_fd_table[fd].offset;
    vfs_node_t *node = g_fd_table[fd].node;

    switch (whence) {
        case SEEK_SET:
            new_off = (uint32_t)offset;
            break;
        case SEEK_CUR:
            new_off += offset;
            break;
        case SEEK_END:
            if (node) new_off = node->size + offset;
            break;
        default:
            return -1;
    }

    g_fd_table[fd].offset = new_off;
    return (int32_t)new_off;
}

int vfs_ioctl(int fd, uint32_t cmd, void *arg) {
    if (fd < 0 || fd >= MAX_FDS || !g_fd_table[fd].in_use) return -1;

    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        pty_t *p = pty_get_active();
        if (!p) return -1;
        switch (cmd) {
            case TCGETS:
                if (arg) memcpy(arg, &p->termios, sizeof(struct termios));
                return 0;
            case TCSETS:
                if (arg) memcpy(&p->termios, arg, sizeof(struct termios));
                return 0;
            case TIOCGWINSZ:
                if (arg) memcpy(arg, &p->winsize, sizeof(struct winsize));
                return 0;
            case TIOCSWINSZ:
                if (arg) memcpy(&p->winsize, arg, sizeof(struct winsize));
                return 0;
            default:
                return -1;
        }
    }

    vfs_node_t *node = g_fd_table[fd].node;
    if (node && node->ops && node->ops->ioctl) {
        return node->ops->ioctl(node, cmd, arg);
    }
    return -1;
}

int vfs_isatty(int fd) {
    if (fd < 0 || fd >= MAX_FDS || !g_fd_table[fd].in_use) return 0;
    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) return 1;

    vfs_node_t *node = g_fd_table[fd].node;
    return (node && node->type == VFS_TYPE_CHARDEV);
}
