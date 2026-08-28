/**
 * @file    stax_platform.c
 * @author  shadcy
 * @brief   Standalone User-Space Platform Layer for linuxdoom-1.10 on STAX GPOS.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include <stdint.h>
#include <stddef.h>

/* DOOM headers */
#include "doomdef.h"
#include "doomtype.h"
#include "doomstat.h"
#include "d_event.h"
#include "v_video.h"
#include "i_video.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_net.h"
#include "d_net.h"
#include "g_game.h"
#include "m_misc.h"
#include "m_argv.h"

/* ============================================================================
 * Userland Output & Console Primitives
 * ============================================================================ */
static void u_putc(char c) {
    u_write(STDOUT_FILENO, &c, 1);
}

static void u_puts(const char *s) {
    if (!s) return;
    size_t len = 0;
    while (s[len]) len++;
    u_write(STDOUT_FILENO, s, len);
}

/* ============================================================================
 * String & Memory Primitives
 * ============================================================================ */

void *stax_memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *stax_memcpy(void *d, const void *s, size_t n) {
    unsigned char *dst = (unsigned char *)d;
    const unsigned char *src = (const unsigned char *)s;
    while (n--) *dst++ = *src++;
    return d;
}

size_t stax_strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

char *stax_strcpy(char *d, const char *s) {
    char *orig = d;
    while ((*d++ = *s++));
    return orig;
}

char *stax_strncpy(char *d, const char *s, size_t n) {
    char *orig = d;
    while (n > 0 && *s) {
        *d++ = *s++;
        n--;
    }
    while (n > 0) {
        *d++ = '\0';
        n--;
    }
    return orig;
}

int stax_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int stax_strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *stax_strcat(char *d, const char *s) {
    char *orig = d;
    while (*d) d++;
    while ((*d++ = *s++));
    return orig;
}

int stax_memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *a = (const unsigned char *)s1;
    const unsigned char *b = (const unsigned char *)s2;
    while (n--) {
        if (*a != *b) return *a - *b;
        a++; b++;
    }
    return 0;
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    while (n > 0) {
        int c1 = (unsigned char)*s1;
        int c2 = (unsigned char)*s2;
        int a = (c1 >= 'a' && c1 <= 'z') ? c1 - 32 : c1;
        int b = (c2 >= 'a' && c2 <= 'z') ? c2 - 32 : c2;
        if (a != b) return a - b;
        if (c1 == '\0') return 0;
        s1++;
        s2++;
        n--;
    }
    return 0;
}

int stax_strcasecmp(const char *s1, const char *s2) {
    while (*s1 || *s2) {
        int c1 = (unsigned char)*s1;
        int c2 = (unsigned char)*s2;
        int a = (c1 >= 'a' && c1 <= 'z') ? c1 - 32 : c1;
        int b = (c2 >= 'a' && c2 <= 'z') ? c2 - 32 : c2;
        if (a != b) return a - b;
        s1++;
        s2++;
    }
    return 0;
}

char *stax_strchr(const char *s, int c) {
    while (*s) { if (*s == c) return (char *)s; s++; }
    return (c == 0) ? (char *)s : (char *)0;
}

int stax_atoi(const char *s) {
    int result = 0, neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') { result = result * 10 + (*s - '0'); s++; }
    return neg ? -result : result;
}

static void uint_to_str(unsigned int n, char *buf, int base, int *len) {
    char tmp[12]; int i = 0; int j;
    if (n == 0) { tmp[i++] = '0'; }
    while (n) { int d = n % base; tmp[i++] = (d < 10) ? '0' + d : 'a' + d - 10; n /= base; }
    *len = i;
    for (j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
}

static void emit_char(char *out, char **cursor, char c) {
    if (out) *(*cursor)++ = c;
    else u_putc(c);
}

static void emit_str(char *out, char **cursor, const char *s, int width, int prec, int zero_pad) {
    int len = 0;
    if (!s) s = "(null)";
    while (s[len] && (prec < 0 || len < prec)) len++;
    if (len < width) {
        char pad = zero_pad ? '0' : ' ';
        while (len < width) { emit_char(out, cursor, pad); len++; }
    }
    int count = 0;
    while (*s && (prec < 0 || count < prec)) {
        emit_char(out, cursor, *s++);
        count++;
    }
}

static void emit_uint(char *out, char **cursor, unsigned int v, int base, int width, int zero_pad) {
    char tmp[16]; int len; int i;
    uint_to_str(v, tmp, base, &len);
    if ((int)len < width) {
        char pad = zero_pad ? '0' : ' ';
        while (len < width) { emit_char(out, cursor, pad); len++; }
    }
    for (i = 0; i < len; i++) emit_char(out, cursor, tmp[i]);
}

int stax_vsprintf(char *buf, const char *fmt, va_list args) {
    char *out = buf;
    char *cursor = buf;

    while (*fmt) {
        if (*fmt != '%') {
            emit_char(out, &cursor, *fmt++);
            continue;
        }
        fmt++;

        int zero_pad = 0;
        int width = 0;
        int prec = -1;
        if (*fmt == '0') { zero_pad = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            zero_pad = 1;
            while (*fmt >= '0' && *fmt <= '9')
                prec = prec * 10 + (*fmt++ - '0');
            if (prec > width) width = prec;
        }

        switch (*fmt) {
            case 's': emit_str(out, &cursor, va_arg(args, const char *), width, prec, zero_pad); break;
            case 'c': emit_char(out, &cursor, (char)va_arg(args, int)); break;
            case 'd': case 'i': {
                int val = va_arg(args, int);
                if (val < 0) { emit_char(out, &cursor, '-'); val = -val; if (width > 0) width--; }
                emit_uint(out, &cursor, (unsigned int)val, 10, width, zero_pad);
                break;
            }
            case 'u': emit_uint(out, &cursor, va_arg(args, unsigned int), 10, width, zero_pad); break;
            case 'x': case 'X': case 'p': emit_uint(out, &cursor, va_arg(args, unsigned int), 16, width, zero_pad); break;
            case '%': emit_char(out, &cursor, '%'); break;
            default: break;
        }
        fmt++;
    }
    if (out) *cursor = '\0';
    return out ? (int)(cursor - buf) : 0;
}

int stax_sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = stax_vsprintf(buf, fmt, args);
    va_end(args);
    return n;
}

void stax_kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    stax_vsprintf((char *)0, fmt, args);
    va_end(args);
}

char kgetc(void) {
    char c = 0;
    while (u_read(STDIN_FILENO, &c, 1) <= 0) {}
    return c;
}

/* ============================================================================
 * Memory Pool & Zone Allocation
 * ============================================================================ */
#define ZONE_SIZE  (4 * 1024 * 1024)   /* 4 MB for Z_Zone */
#define SLAB_SIZE  (1 * 1024 * 1024)   /* 1 MB for malloc slab */

static unsigned char doom_zone_buf[ZONE_SIZE] __attribute__((aligned(8)));
static unsigned char doom_slab_buf[SLAB_SIZE] __attribute__((aligned(8)));
static unsigned int  doom_slab_pos = 0;

void *stax_doom_malloc(size_t size) {
    if (size == 0) return (void*)0;
    size = (size + 7) & ~7;
    if ((unsigned)doom_slab_pos + (unsigned)size > SLAB_SIZE) {
        u_puts("[DOOM] malloc OOM\n");
        return (void*)0;
    }
    void *p = &doom_slab_buf[doom_slab_pos];
    doom_slab_pos += (unsigned)size;
    return p;
}

void *stax_doom_realloc(void *ptr, size_t newsize) {
    void *p = stax_doom_malloc(newsize);
    if (p && ptr) stax_memcpy(p, ptr, newsize);
    return p;
}

void stax_doom_free(void *ptr) { (void)ptr; }
void *stax_doom_alloca(size_t size) { return stax_doom_malloc(size); }

int I_GetHeapSize(void) { return ZONE_SIZE; }
byte *I_ZoneBase(int *size) {
    *size = ZONE_SIZE;
    return (byte *)doom_zone_buf;
}

/* ============================================================================
 * Time & Frame Ticks
 * ============================================================================ */
static unsigned int doom_time_offset = 0;

void doom_reset_time(void) {
    doom_time_offset = u_uptime();
}

int I_GetTime(void) {
    unsigned int t = u_uptime();
    if (t < doom_time_offset) return 0;
    return (int)((t - doom_time_offset) * 7 / 200); /* 35 tics/sec */
}

void I_Init(void) {}
void I_Quit(void) { u_exit(0); }
void I_WaitVBL(int count) { (void)count; }
void I_BeginRead(void) {}
void I_EndRead(void) {}
byte *I_AllocLow(int length) { return (byte *)stax_doom_malloc(length); }
void I_Tactile(int on, int off, int total) { (void)on; (void)off; (void)total; }

ticcmd_t  emptycmd;
ticcmd_t *I_BaseTiccmd(void) { return &emptycmd; }

char *sndserver_filename = (char*)0;
int mb_used = 0;

void I_Error(char *error, ...) {
    va_list args;
    u_puts("\n[DOOM ERROR] ");
    va_start(args, error);
    stax_vsprintf((char*)0, error, args);
    va_end(args);
    u_putc('\n');
    u_exit(1);
}

/* ============================================================================
 * Direct Video Blitting to Framebuffer (/dev/fb0)
 * ============================================================================ */
#define DOOM_W   320
#define DOOM_H   200
#define DOOM_MAX_SCALE 3

static uint16_t doom_palette[256];
static uint16_t *doom_fb = NULL;
static uint32_t doom_fb_w = 1024;
static uint32_t doom_fb_h = 768;
static uint16_t doom_scaled_line[DOOM_W * DOOM_MAX_SCALE];

typedef struct {
    uint32_t xres;
    uint32_t yres;
    uint32_t bits_per_pixel;
    uint32_t smem_start;
    uint32_t smem_len;
} fb_var_screeninfo_t;

static void draw_rect(int x, int y, int w, int h, uint16_t color) {
    int r, c;
    if (!doom_fb || x < 0 || y < 0 || x + w > (int)doom_fb_w || y + h > (int)doom_fb_h) return;
    for (r = 0; r < h; r++) {
        uint16_t *dst = doom_fb + (y + r) * doom_fb_w + x;
        for (c = 0; c < w; c++) dst[c] = color;
    }
}

static void draw_btn(int bx, int by, uint16_t col) {
    draw_rect(bx + 2, by, 10, 14, col);
    draw_rect(bx, by + 2, 14, 10, col);
    draw_rect(bx + 1, by + 1, 12, 12, col);
}

void I_InitGraphics(void) {
    int i;
    for (i = 0; i < 256; i++) {
        uint8_t v = (uint8_t)i;
        doom_palette[i] = ((v >> 3) << 11) | ((v >> 2) << 5) | (v >> 3);
    }

    int fb_fd = u_open("/dev/fb0", 2);
    if (fb_fd >= 0) {
        fb_var_screeninfo_t info;
        if (u_ioctl(fb_fd, 0x4600, &info) == 0) {
            doom_fb = (uint16_t *)info.smem_start;
            doom_fb_w = info.xres;
            doom_fb_h = info.yres;
        }
        u_close(fb_fd);
    }
    if (!doom_fb) {
        doom_fb = (uint16_t *)0x01C00000;
        doom_fb_w = 1024;
        doom_fb_h = 768;
    }

    /* Clear screen to black for immersive full-screen presentation */
    draw_rect(0, 0, (int)doom_fb_w, (int)doom_fb_h, 0x0000);
}

void I_ShutdownGraphics(void) {}
void I_StartFrame(void) {}
void I_UpdateNoBlit(void) {}

void I_FinishUpdate(void) {
    if (!doom_fb || !screens[0]) return;

    /* 3x Integer Scaling: 320x200 -> 960x600 centered on 1024x768 */
    int scale = 3;
    int scaled_w = DOOM_W * scale; /* 960 */
    int scaled_h = DOOM_H * scale; /* 600 */
    int dest_x = (doom_fb_w > (uint32_t)scaled_w) ? (int)(doom_fb_w - scaled_w) / 2 : 0;
    int dest_y = (doom_fb_h > (uint32_t)scaled_h) ? (int)(doom_fb_h - scaled_h) / 2 : 0;

    byte *src = screens[0];
    int y, x, sx, sy;
    for (y = 0; y < DOOM_H; y++) {
        byte *row_src = src + y * DOOM_W;
        int out = 0;
        for (x = 0; x < DOOM_W; x++) {
            uint16_t color = doom_palette[row_src[x]];
            for (sx = 0; sx < scale; sx++)
                doom_scaled_line[out++] = color;
        }
        for (sy = 0; sy < scale; sy++) {
            uint16_t *row_dst = doom_fb + (dest_y + y * scale + sy) * doom_fb_w + dest_x;
            stax_memcpy(row_dst, doom_scaled_line, (size_t)scaled_w * sizeof(uint16_t));
            if (doom_fb != (uint16_t *)0x01C00000) {
                uint16_t *scanout_dst = (uint16_t *)0x01C00000 + (dest_y + y * scale + sy) * doom_fb_w + dest_x;
                stax_memcpy(scanout_dst, doom_scaled_line, (size_t)scaled_w * sizeof(uint16_t));
            }
        }
    }
}

void I_ReadScreen(byte *scr) {
    stax_memcpy(scr, screens[0], DOOM_W * DOOM_H);
}

void I_SetPalette(byte *palette) {
    int i;
    for (i = 0; i < 256; i++) {
        uint8_t r = *palette++;
        uint8_t g = *palette++;
        uint8_t b = *palette++;
        doom_palette[i] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }
}

/* ============================================================================
 * Sound & Network Stubs
 * ============================================================================ */
void I_InitSound(void)   {}
void I_ShutdownSound(void) {}
void I_ShutdownMusic(void) {}
void I_InitMusic(void)   {}
void I_SetChannels(void) {}
int  I_GetSfxLumpNum(sfxinfo_t *sfx) { (void)sfx; return 0; }
int  I_StartSound(int id,int vol,int sep,int pitch,int priority) { (void)id;(void)vol;(void)sep;(void)pitch;(void)priority; return 0; }
void I_StopSound(int handle) { (void)handle; }
int  I_SoundIsPlaying(int handle) { (void)handle; return 0; }
void I_UpdateSoundParams(int h,int v,int s,int p) { (void)h;(void)v;(void)s;(void)p; }
void I_UpdateSound(void) {}
void I_SubmitSound(void) {}
int  I_RegisterSong(void *data) { (void)data; return 0; }
void I_PlaySong(int handle, int looping) { (void)handle;(void)looping; }
void I_PauseSong(int handle) { (void)handle; }
void I_ResumeSong(int handle) { (void)handle; }
void I_StopSong(int handle) { (void)handle; }
void I_UnRegisterSong(int handle) { (void)handle; }
void I_SetMusicVolume(int volume) { (void)volume; }

void I_InitNetwork(void) {
    doomcom = (doomcom_t *)stax_doom_malloc(sizeof(doomcom_t));
    if (!doomcom) I_Error("I_InitNetwork: out of memory");
    stax_memset(doomcom, 0, sizeof(*doomcom));
    netgame = false;
    doomcom->id = DOOMCOM_ID;
    doomcom->numplayers = 1;
    doomcom->numnodes = 1;
    doomcom->deathmatch = false;
    doomcom->consoleplayer = 0;
    doomcom->ticdup = 1;
    doomcom->extratics = 0;
}
void I_NetCmd(void) {}

/* ============================================================================
 * Keyboard Input Processing
 * ============================================================================ */
static int translate_key(char c, int *doom_key) {
    switch (c) {
        case 'w': case 'W': *doom_key = KEY_UPARROW;    return 1;
        case 's': case 'S': *doom_key = KEY_DOWNARROW;  return 1;
        case 'a': case 'A': *doom_key = KEY_LEFTARROW;  return 1;
        case 'd': case 'D': *doom_key = KEY_RIGHTARROW; return 1;
        case 'f': case 'F': *doom_key = KEY_RCTRL;      return 1;
        case 'e': case 'E': *doom_key = KEY_RSHIFT;     return 1;
        case 'z': case 'Z': *doom_key = ',';            return 1;
        case 'x': case 'X': *doom_key = '.';            return 1;
        case '\r': case '\n': *doom_key = KEY_ENTER;    return 1;
        case '\033': *doom_key = KEY_ESCAPE;             return 1;
        case ' ':   *doom_key = ' ';                     return 1;
        case '\b':  *doom_key = KEY_BACKSPACE;           return 1;
        case 0x11: *doom_key = KEY_UPARROW;     return 1;
        case 0x12: *doom_key = KEY_DOWNARROW;   return 1;
        case 0x13: *doom_key = KEY_LEFTARROW;   return 1;
        case 0x14: *doom_key = KEY_RIGHTARROW;  return 1;
        default:
            if (c >= 'a' && c <= 'z') { *doom_key = c; return 1; }
            if (c >= '1' && c <= '9') { *doom_key = c; return 1; }
            if (c == '0')             { *doom_key = '0'; return 1; }
            return 0;
    }
}

static int held_keys[256];

void I_StartTic(void) {
    event_t ev;
    int dk, k;
    char c;

    /* Release keys held for previous tic */
    for (k = 0; k < 256; k++) {
        if (held_keys[k] > 0) {
            held_keys[k]--;
            if (held_keys[k] == 0) {
                ev.type = ev_keyup;
                ev.data1 = k;
                D_PostEvent(&ev);
            }
        }
    }

    while (u_read(STDIN_FILENO, &c, 1) > 0) {
        if (translate_key(c, &dk)) {
            ev.type  = ev_keydown;
            ev.data1 = dk;
            D_PostEvent(&ev);
            if (dk >= 0 && dk < 256) {
                held_keys[dk] = 3; /* Hold key active for 3 tics */
            }
        }
    }
}

/* ============================================================================
 * VFS-backed File I/O for WAD loading
 * ============================================================================ */

static void path_to_fat(const char *path, char *out) {
    const char *p = path;
    const char *q;
    for (q = path; *q; q++)
        if (*q == '/' || *q == '\\') p = q + 1;
    int i = 0;
    while (*p && i < 11) { out[i++] = (char)stax_toupper(*p++); }
    out[i] = '\0';
}

int stax_access(const char *pathname, int mode) {
    (void)mode;
    int fd = stax_open(pathname, 0);
    if (fd >= 0) {
        stax_close(fd);
        return 0;
    }
    return -1;
}

int stax_open(const char *path, int flags, ...) {
    (void)flags;
    char fatname[16];
    stax_memset(fatname, 0, sizeof(fatname));
    path_to_fat(path, fatname);
    int fd = u_open(fatname, 0);
    if (fd < 0) {
        char alt[20];
        stax_memset(alt, 0, sizeof(alt));
        alt[0] = '/';
        stax_strncpy(alt + 1, fatname, 15);
        fd = u_open(alt, 0);
    }
    if (fd < 0 && stax_strcmp(fatname, "DOOM1.WAD") == 0) {
        fd = u_open("DOOM.WAD", 0);
        if (fd < 0) fd = u_open("/DOOM.WAD", 0);
    }
    return fd;
}

int stax_read(int fd, void *buf, int count) {
    return u_read(fd, buf, count);
}

int stax_lseek(int fd, int offset, int whence) {
    return u_lseek(fd, offset, whence);
}

int stax_close(int fd) {
    return u_close(fd);
}

int stax_fstat(int fd, void *statbuf) {
    struct { int st_size; int st_mode; int st_uid; } *sb = statbuf;
    int cur = u_lseek(fd, 0, 1);
    int end = u_lseek(fd, 0, 2);
    u_lseek(fd, cur, 0);
    sb->st_size = end;
    sb->st_mode = 0;
    sb->st_uid  = 0;
    return 0;
}

/* ============================================================================
 * Userland DOOM Standalone Main Entry Point
 * ============================================================================ */
extern void D_DoomMain(void);
extern boolean singletics;
volatile int stax_doom_quit_requested = 0;

static char *doom_argv_static[5];
static char  doom_name[]  = "doom.elf";
static char  doom_wad[]   = "DOOM1.WAD";
static char  doom_nomus[] = "-nomusic";
static char  doom_nosnd[] = "-nosound";

int main(int argc, char **argv) {
    u_puts("\n========================================\n");
    u_puts("  STAX GPOS: DOOM Userland Process\n");
    u_puts("  Running in USR Mode (0x10) @ 0x08000000\n");
    u_puts("========================================\n\n");

    singletics = true;

    doom_argv_static[0] = doom_name;
    doom_argv_static[1] = (argc > 1 && argv && argv[1]) ? argv[1] : doom_wad;
    doom_argv_static[2] = doom_nomus;
    doom_argv_static[3] = doom_nosnd;
    doom_argv_static[4] = NULL;

    myargc = 4;
    myargv = doom_argv_static;

    doom_reset_time();
    D_DoomMain();

    u_exit(0);
    return 0;
}
