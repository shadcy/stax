/**
 * @file    devfs.c
 * @author  shadcy
 * @brief   Device Filesystem (/dev) Node Registry & Standard Drivers for STAX.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include "devfs.h"
#include "framebuffer.h"
#include "pty.h"
#include "rtc.h"
#include "string.h"
#include "heap.h"
#include "console.h"

extern volatile unsigned int tick_count;

static vfs_node_t g_devfs_root;
static vfs_node_t g_dev_nodes[16];
static int        g_dev_node_count = 0;

/* ============================================================================
 * /dev/null Driver
 * ============================================================================ */

static int32_t dev_null_read(vfs_node_t *node, uint32_t offset, void *buf, size_t count) {
    (void)node; (void)offset; (void)buf; (void)count;
    return 0; /* EOF */
}

static int32_t dev_null_write(vfs_node_t *node, uint32_t offset, const void *buf, size_t count) {
    (void)node; (void)offset; (void)buf;
    return (int32_t)count; /* Discard bytes */
}

static vfs_ops_t g_dev_null_ops = {
    .open  = NULL,
    .close = NULL,
    .read  = dev_null_read,
    .write = dev_null_write,
    .ioctl = NULL,
    .stat  = NULL
};

/* ============================================================================
 * /dev/zero Driver
 * ============================================================================ */

static int32_t dev_zero_read(vfs_node_t *node, uint32_t offset, void *buf, size_t count) {
    (void)node; (void)offset;
    if (!buf) return -1;
    memset(buf, 0, count);
    return (int32_t)count;
}

static int32_t dev_zero_write(vfs_node_t *node, uint32_t offset, const void *buf, size_t count) {
    (void)node; (void)offset; (void)buf;
    return (int32_t)count;
}

static vfs_ops_t g_dev_zero_ops = {
    .open  = NULL,
    .close = NULL,
    .read  = dev_zero_read,
    .write = dev_zero_write,
    .ioctl = NULL,
    .stat  = NULL
};

/* ============================================================================
 * /dev/urandom Driver (Hardware-seeded PRNG)
 * ============================================================================ */

static uint32_t g_prng_seed = 0x19930717;

static int32_t dev_urandom_read(vfs_node_t *node, uint32_t offset, void *buf, size_t count) {
    (void)node; (void)offset;
    if (!buf) return -1;
    uint8_t *p = (uint8_t *)buf;

    for (size_t i = 0; i < count; i++) {
        g_prng_seed = g_prng_seed * 1103515245 + 12345 + tick_count;
        p[i] = (uint8_t)((g_prng_seed >> 16) & 0xFF);
    }
    return (int32_t)count;
}

static int32_t dev_urandom_write(vfs_node_t *node, uint32_t offset, const void *buf, size_t count) {
    (void)node; (void)offset;
    if (buf && count >= 4) {
        memcpy(&g_prng_seed, buf, 4);
    }
    return (int32_t)count;
}

static vfs_ops_t g_dev_urandom_ops = {
    .open  = NULL,
    .close = NULL,
    .read  = dev_urandom_read,
    .write = dev_urandom_write,
    .ioctl = NULL,
    .stat  = NULL
};

/* ============================================================================
 * /dev/tty0 Driver (Active Pseudo-Terminal)
 * ============================================================================ */

static int32_t dev_tty_read(vfs_node_t *node, uint32_t offset, void *buf, size_t count) {
    (void)node; (void)offset;
    pty_t *p = pty_get_active();
    if (p) {
        return pty_slave_read(p, (char *)buf, count);
    }
    return 0;
}

static int32_t dev_tty_write(vfs_node_t *node, uint32_t offset, const void *buf, size_t count) {
    (void)node; (void)offset;
    pty_t *p = pty_get_active();
    if (p) {
        return pty_slave_write(p, (const char *)buf, count);
    }
    const char *s = (const char *)buf;
    for (size_t i = 0; i < count; i++) kputc(s[i]);
    return (int32_t)count;
}

static int dev_tty_ioctl(vfs_node_t *node, uint32_t cmd, void *arg) {
    (void)node;
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

static vfs_ops_t g_dev_tty_ops = {
    .open  = NULL,
    .close = NULL,
    .read  = dev_tty_read,
    .write = dev_tty_write,
    .ioctl = dev_tty_ioctl,
    .stat  = NULL
};

/* ============================================================================
 * /dev/fb0 Driver (Direct Framebuffer Device)
 * ============================================================================ */

#define FBIOGET_VSCREENINFO 0x4600

typedef struct {
    uint32_t xres;
    uint32_t yres;
    uint32_t bits_per_pixel;
    uint32_t smem_start;
    uint32_t smem_len;
} fb_var_screeninfo_t;

static int32_t dev_fb_read(vfs_node_t *node, uint32_t offset, void *buf, size_t count) {
    (void)node;
    uint16_t *fbuf = fb_get_buffer();
    if (!fbuf || !buf) return -1;
    uint32_t max_bytes = fb_width * fb_height * 2;
    if (offset >= max_bytes) return 0;
    if (offset + count > max_bytes) count = max_bytes - offset;

    memcpy(buf, (const uint8_t *)fbuf + offset, count);
    return (int32_t)count;
}

static int32_t dev_fb_write(vfs_node_t *node, uint32_t offset, const void *buf, size_t count) {
    (void)node;
    uint16_t *fbuf = fb_get_buffer();
    if (!fbuf || !buf) return -1;
    uint32_t max_bytes = fb_width * fb_height * 2;
    if (offset >= max_bytes) return 0;
    if (offset + count > max_bytes) count = max_bytes - offset;

    memcpy((uint8_t *)fbuf + offset, buf, count);
    return (int32_t)count;
}

static int dev_fb_ioctl(vfs_node_t *node, uint32_t cmd, void *arg) {
    (void)node;
    if (cmd == FBIOGET_VSCREENINFO && arg) {
        fb_var_screeninfo_t *info = (fb_var_screeninfo_t *)arg;
        info->xres = fb_width;
        info->yres = fb_height;
        info->smem_start = 0x01C00000; /* Direct physical LCD scanout (PL110 FB_BASE) */
        info->smem_len = fb_width * fb_height * 2;
        return 0;
    }
    return -1;
}

static vfs_ops_t g_dev_fb_ops = {
    .open  = NULL,
    .close = NULL,
    .read  = dev_fb_read,
    .write = dev_fb_write,
    .ioctl = dev_fb_ioctl,
    .stat  = NULL
};

/* ============================================================================
 * Devfs Initialization & Registry
 * ============================================================================ */

vfs_node_t *devfs_get_root(void) {
    return &g_devfs_root;
}

int devfs_register_device(const char *name, uint32_t type, vfs_ops_t *ops, void *priv_data) {
    if (!name || g_dev_node_count >= 16) return -1;

    vfs_node_t *node = &g_dev_nodes[g_dev_node_count++];
    memset(node, 0, sizeof(vfs_node_t));
    strncpy(node->name, name, sizeof(node->name) - 1);
    node->type      = type;
    node->ops       = ops;
    node->priv_data = priv_data;

    /* Append to children list of /dev root */
    node->next = g_devfs_root.children;
    g_devfs_root.children = node;
    return 0;
}

void devfs_init(void) {
    memset(&g_devfs_root, 0, sizeof(vfs_node_t));
    strcpy(g_devfs_root.name, "dev");
    g_devfs_root.type = VFS_TYPE_DIR;
    g_dev_node_count  = 0;

    /* Register standard character and block devices */
    devfs_register_device("null",    VFS_TYPE_CHARDEV, &g_dev_null_ops,    NULL);
    devfs_register_device("zero",    VFS_TYPE_CHARDEV, &g_dev_zero_ops,    NULL);
    devfs_register_device("urandom", VFS_TYPE_CHARDEV, &g_dev_urandom_ops, NULL);
    devfs_register_device("tty0",    VFS_TYPE_CHARDEV, &g_dev_tty_ops,     NULL);
    devfs_register_device("tty",     VFS_TYPE_CHARDEV, &g_dev_tty_ops,     NULL);
    devfs_register_device("fb0",     VFS_TYPE_CHARDEV, &g_dev_fb_ops,      NULL);

    /* Mount at /dev */
    vfs_mount("/dev", &g_devfs_root);
    kputs("DEVFS: Device Filesystem mounted at /dev (/dev/null, /dev/zero, /dev/urandom, /dev/tty0, /dev/fb0).\n");
}
