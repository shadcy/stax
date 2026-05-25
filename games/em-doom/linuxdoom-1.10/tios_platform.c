/* ============================================================================
 * tios_platform.c — T-OS platform layer for linuxdoom-1.10
 *this is the most  shitty thing youll ever encounter my friend trust me
 * Implements:
 *   - i_system   : timer, memory zone, error, quit
 *   - i_video    : framebuffer blit (320x200 centered in 640x480)
 *   - i_sound    : all stubs {for future implementation i guess}
 *   - i_net      : all stubs
 *   - POSIX shim : open/read/lseek/close → FAT, malloc → static pool (idk cursor agent cooked this too well)
 *   - String/mem : memcpy, memset, strlen, sprintf, etc.
 * ============================================================================ */

/* Pull in T-OS includes BEFORE tios_compat.h redefines everything */
#include "../../../include/framebuffer.h"
#include "../../../include/keyboard.h"
#include "../../../include/font8x16.h"
#include "../../../include/fat.h"
#include "../../../include/console.h"
#include "../../../include/gfx_console.h"

#include <stdint.h>
#include <stddef.h>

/* Now include DOOM headers (after T-OS includes to avoid conflicts) */
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
#include "doomstat.h"
#include "m_misc.h"
#include "m_argv.h"

/* ============================================================================
 * External T-OS symbols
 * ============================================================================ */
extern volatile unsigned int tick_count;  /* 1000 Hz kernel timer */

/* console.c */
extern void  kputs(const char *s);
extern void  kputc(char c);
/* keyboard.h exposes kb_getevent() for press/release events */
extern int  kb_getevent(void);
extern void  kput_uint(unsigned int n);

/* ============================================================================
 * Global quit flag
 * ============================================================================ */
volatile int tios_doom_quit_requested = 0;
int          tios_errno = 0;

/* ============================================================================
 * String / memory functions (replaces glibc)
 * ============================================================================ */
void *tios_memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

#undef memcpy
#undef strcpy

void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *dst = (unsigned char *)d;
    const unsigned char *src = (const unsigned char *)s;
    while (n--) *dst++ = *src++;
    return d;
}

void *tios_memcpy(void *d, const void *s, size_t n) { return memcpy(d, s, n); }


int tios_memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *a = (const unsigned char *)s1;
    const unsigned char *b = (const unsigned char *)s2;
    while (n--) {
        if (*a != *b) return *a - *b;
        a++; b++;
    }
    return 0;
}

size_t tios_strlen(const char *s)
{
    size_t n = 0;
    while (*s++) n++;
    return n;
}

char *tios_strncpy(char *d, const char *s, size_t n)
{
    char *dst = d;
    while (n && *s) { *dst++ = *s++; n--; }
    while (n--) *dst++ = 0;
    return d;
}

int tios_strncmp(const char *s1, const char *s2, size_t n)
{
    while (n && *s1 && *s1 == *s2) { s1++; s2++; n--; }
    if (!n) return 0;
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *strcpy(char *d, const char *s)
{
    char *dst = d;
    while ((*dst++ = *s++));
    return d;
}

char *strcat(char *d, const char *s)
{
    char *dst = d;
    while (*dst) dst++;
    while ((*dst++ = *s++));
    return d;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}



int strncasecmp(const char *s1, const char *s2, size_t n)
{
    while (n && *s1 && *s1 == *s2) { s1++; s2++; n--; }
    if (!n) return 0;
    int a = (*s1 >= 'a' && *s1 <= 'z') ? *s1 - 32 : *s1;
    int b = (*s2 >= 'a' && *s2 <= 'z') ? *s2 - 32 : *s2;
    return a - b;
}

int tios_strcasecmp(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        int a = (*s1 >= 'a' && *s1 <= 'z') ? *s1 - 32 : *s1;
        int b = (*s2 >= 'a' && *s2 <= 'z') ? *s2 - 32 : *s2;
        if (a != b) return a - b;
        s1++; s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *tios_strchr(const char *s, int c)
{
    while (*s) { if (*s == c) return (char *)s; s++; }
    return (c == 0) ? (char *)s : (char *)0;
}

int tios_atoi(const char *s)
{
    int result = 0, neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') { result = result * 10 + (*s - '0'); s++; }
    return neg ? -result : result;
}

/* ---- Minimal sprintf/printf ---- */
static void uint_to_str(unsigned int n, char *buf, int base, int *len)
{
    char tmp[12]; int i = 0;
    if (n == 0) { tmp[i++] = '0'; }
    while (n) { int d = n % base; tmp[i++] = (d < 10) ? '0' + d : 'a' + d - 10; n /= base; }
    *len = i;
    for (int j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
}

static void emit_char(char *out, char **cursor, char c)
{
    if (out) *(*cursor)++ = c;
    else kputc(c);
}

static void emit_str(char *out, char **cursor, const char *s, int width, int zero_pad)
{
    int len = 0;
    if (!s) s = "(null)";
    while (s[len]) len++;

    if (len < width) {
        char pad = zero_pad ? '0' : ' ';
        while (len < width) {
            emit_char(out, cursor, pad);
            len++;
        }
    }
    while (*s) emit_char(out, cursor, *s++);
}

static void emit_uint(char *out, char **cursor, unsigned int v, int base, int width, int zero_pad)
{
    char tmp[16];
    int len;
    uint_to_str(v, tmp, base, &len);
    if ((int)len < width) {
        char pad = zero_pad ? '0' : ' ';
        while (len < width) {
            emit_char(out, cursor, pad);
            len++;
        }
    }
    for (int i = 0; i < len; i++)
        emit_char(out, cursor, tmp[i]);
}

int tios_vsprintf(char *buf, const char *fmt, va_list args)
{
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
        if (*fmt == '0') { zero_pad = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');
        if (*fmt == '.') {
            fmt++;
            int prec = 0;
            while (*fmt >= '0' && *fmt <= '9')
                prec = prec * 10 + (*fmt++ - '0');
            if (prec > width)
                width = prec;
            zero_pad = 1;
        }
        if (*fmt == 'l')
            fmt++;

        switch (*fmt) {
            case 'd': case 'i': {
                int v = va_arg(args, int);
                if (v < 0) {
                    emit_char(out, &cursor, '-');
                    if (width > 0) width--;
                    emit_uint(out, &cursor, (unsigned)(-v), 10, width, zero_pad);
                } else {
                    emit_uint(out, &cursor, (unsigned)v, 10, width, zero_pad);
                }
                break;
            }
            case 'u':
                emit_uint(out, &cursor, va_arg(args, unsigned), 10, width, zero_pad);
                break;
            case 'x': case 'X':
                emit_uint(out, &cursor, va_arg(args, unsigned), 16, width, zero_pad);
                break;
            case 'p': {
                emit_char(out, &cursor, '0');
                emit_char(out, &cursor, 'x');
                emit_uint(out, &cursor,
                          (unsigned)(uintptr_t)va_arg(args, void *), 16, width, zero_pad);
                break;
            }
            case 's':
                emit_str(out, &cursor, va_arg(args, const char *), width, zero_pad);
                break;
            case 'c':
                emit_char(out, &cursor, (char)va_arg(args, int));
                break;
            case '%':
                emit_char(out, &cursor, '%');
                break;
            default:
                break;
        }
        fmt++;
    }
    if (out)
        *cursor = '\0';
    return out ? (int)(cursor - buf) : 0;
}

int tios_sprintf(char *buf, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = tios_vsprintf(buf, fmt, args);
    va_end(args);
    return n;
}

void tios_kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    tios_vsprintf((char*)0, fmt, args);
    va_end(args);
}

/* ============================================================================
 * Static Memory Pool for DOOM allocations (1.5 MB)
 * ============================================================================ */
#define POOL_SIZE  (8 * 1024 * 1024)   /* 8 MB */
#define ZONE_SIZE  (6 * 1024 * 1024)   /* 6 MB for Z_Zone */
#define SLAB_SIZE  (2 * 1024 * 1024)   /* 2 MB for malloc slab */

/* These go in BSS — no flash cost */
static unsigned char doom_zone_buf[ZONE_SIZE] __attribute__((aligned(8)));
static unsigned char doom_slab_buf[SLAB_SIZE] __attribute__((aligned(8)));
static unsigned int  doom_slab_pos = 0;

/* Tiny bump allocator for DOOM's internal malloc calls (lumpinfo, lumpcache, etc.) */
void *tios_doom_malloc(int size)
{
    if (size <= 0) return (void*)0;
    size = (size + 7) & ~7;   /* 8-byte align */
    if ((unsigned)doom_slab_pos + (unsigned)size > SLAB_SIZE) {
        kputs("[DOOM] malloc OOM\n");
        return (void*)0;
    }
    void *p = &doom_slab_buf[doom_slab_pos];
    doom_slab_pos += (unsigned)size;
    return p;
}

/* Realloc: allocate new, copy, abandon old (bump allocator — no free) */
void *tios_doom_realloc(void *ptr, int newsize)
{
    void *p = tios_doom_malloc(newsize);
    if (p && ptr) tios_memcpy(p, ptr, newsize);
    return p;
}

void tios_doom_free(void *ptr) { (void)ptr; /* bump allocator: no-op */ }

/* Stack-style alloca using the slab */
void *tios_doom_alloca(int size)
{
    return tios_doom_malloc(size);
}

/* ============================================================================
 * i_system.c replacements
 * ============================================================================ */
int I_GetHeapSize(void)   { return ZONE_SIZE; }

byte *I_ZoneBase(int *size)
{
    *size = ZONE_SIZE;
    return (byte *)doom_zone_buf;
}

/* DOOM runs at 35 tics/sec; our timer runs at 100 Hz */
int I_GetTime(void)
{
    /* tick_count * 35/1000 = tick_count * 7/200 */
    return (int)(tick_count * 7 / 200);
}

void I_Init(void)
{
    /* Sound: stubbed */
}

static int doom_running = 0;

void I_Quit(void)
{
    doom_running = 0;
    tios_doom_quit_requested = 1;
    /* D_DoomLoop checks tios_doom_quit_requested and returns here */
}

void I_WaitVBL(int count) { (void)count; }
void I_BeginRead(void) {}
void I_EndRead(void) {}

byte *I_AllocLow(int length)
{
    return (byte *)tios_doom_malloc(length);
}

void I_Tactile(int on, int off, int total) { (void)on; (void)off; (void)total; }

ticcmd_t  emptycmd;
ticcmd_t *I_BaseTiccmd(void) { return &emptycmd; }

void I_Error(char *error, ...)
{
    va_list args;
    kputs("[DOOM ERROR] ");
    va_start(args, error);
    tios_vsprintf((char*)0, error, args);
    va_end(args);
    kputc('\n');
    doom_running = 0;
    tios_doom_quit_requested = 1;
    while (1)
	__asm__ volatile("nop");
}

/* ============================================================================
 * i_video.c replacement — 320×200 palette → RGB565, centered in 640×480
 * ============================================================================ */
#define DOOM_W   320
#define DOOM_H   200
#define FB_W     640
#define FB_H     480
#define OFF_X    ((FB_W - DOOM_W) / 2)   /* 160 */
#define OFF_Y    ((FB_H - DOOM_H) / 2)   /* 140 */

/* 256-entry RGB565 palette, updated by I_SetPalette() */
static uint16_t doom_palette[256];

void I_InitGraphics(void)
{
    /* Take over the framebuffer now that DOOM is ready to render.
     * Disable the gfx console here (not at engine startup) so all the
     * WAD-loading / init kputs() messages are visible during loading. */
    gfx_console_enable(0);
    fb_init();
    fb_clear(0x0000);   /* full screen — DOOM only draws the 320x200 window */
    /* Default greyscale palette until DOOM loads its own */
    for (int i = 0; i < 256; i++) {
        uint8_t v = (uint8_t)i;
        doom_palette[i] = ((v >> 3) << 11) | ((v >> 2) << 5) | (v >> 3);
    }
}

void I_ShutdownGraphics(void)
{
    /* nothing to tear down */
}

void I_StartFrame(void) {}

void I_UpdateNoBlit(void) {}

static void draw_overlay_glyph(int px, int py, unsigned char c, uint16_t color, uint16_t bg)
{
    const unsigned char *g = font8x16_data[c];
    uint16_t *fbuf = fb_get_buffer();
    for (int r = 0; r < 16; r++) {
        unsigned char bits = g[r];
        uint16_t *dst = fbuf + (py + r) * FB_WIDTH + px;
        for (int b = 0; b < 8; b++) {
            if (bits & (0x80u >> b)) dst[b] = color;
            else dst[b] = bg;
        }
    }
}

static void draw_overlay_str(int px, int py, const char *s, uint16_t color, uint16_t bg)
{
    while (*s) {
        draw_overlay_glyph(px, py, *s++, color, bg);
        px += 8;
    }
}

void I_FinishUpdate(void)
{
    /* Blit DOOM's 8-bit framebuffer (screens[0]) centred in our 640×480 */
    uint16_t *fb  = fb_get_buffer();
    byte     *src = screens[0];

    for (int y = 0; y < DOOM_H; y++) {
        uint16_t *row = fb + (y + OFF_Y) * FB_W + OFF_X;
        for (int x = 0; x < DOOM_W; x++) {
            row[x] = doom_palette[*src++];
        }
    }

    /* Analytics Overlay */
    static unsigned int last_fps_time = 0;
    static int frames_this_second = 0;
    static int current_fps = 0;

    frames_this_second++;
    if (tick_count - last_fps_time >= 1000) { /* 1000 ticks = 1 sec */
        current_fps = frames_this_second;
        frames_this_second = 0;
        last_fps_time = tick_count;
    }

    char stat_buf[128];
    tios_sprintf(stat_buf, "FPS: %d   |   RAM: %d KB / %d KB       ", 
                 current_fps, doom_slab_pos / 1024, SLAB_SIZE / 1024);
    
    draw_overlay_str(10, 10, stat_buf, COLOR_CYAN, COLOR_BLACK);
}

void I_ReadScreen(byte *scr)
{
    tios_memcpy(scr, screens[0], DOOM_W * DOOM_H);
}

void I_SetPalette(byte *palette)
{
    for (int i = 0; i < 256; i++) {
        uint8_t r = *palette++;
        uint8_t g = *palette++;
        uint8_t b = *palette++;
        /* Apply simple gamma (>> 1 for ~0.5) not needed here — use direct */
        doom_palette[i] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }
}

/* ============================================================================
 * i_sound.c replacement — all stubs
 * ============================================================================ */
void I_InitSound(void)   {}
void I_ShutdownSound(void) {}
void I_ShutdownMusic(void) {}
void I_InitMusic(void)   {}
void I_SetChannels(void) {}
int  I_GetSfxLumpNum(sfxinfo_t *sfx)           { (void)sfx; return 0; }
int  I_StartSound(int id,int vol,int sep,int pitch,int priority)
     { (void)id;(void)vol;(void)sep;(void)pitch;(void)priority; return 0; }
void I_StopSound(int handle)              { (void)handle; }
int  I_SoundIsPlaying(int handle)         { (void)handle; return 0; }
void I_UpdateSoundParams(int h,int v,int s,int p){ (void)h;(void)v;(void)s;(void)p; }
void I_UpdateSound(void)                 {}
void I_SubmitSound(void)                 {}
int  I_RegisterSong(void *data)           { (void)data; return 0; }
void I_PlaySong(int handle, int looping)  { (void)handle;(void)looping; }
void I_PauseSong(int handle)              { (void)handle; }
void I_ResumeSong(int handle)             { (void)handle; }
void I_StopSong(int handle)              { (void)handle; }
void I_UnRegisterSong(int handle)        { (void)handle; }
void I_SetMusicVolume(int volume)         { (void)volume; }

/* ============================================================================
 * i_net.c replacement — single-player only, no networking
 * ============================================================================ */
void I_InitNetwork(void)
{
    doomcom = (doomcom_t *)tios_doom_malloc(sizeof(doomcom_t));
    if (!doomcom)
	I_Error("I_InitNetwork: out of memory");
    tios_memset(doomcom, 0, sizeof(*doomcom));

    netgame = false;
    doomcom->id = DOOMCOM_ID;
    doomcom->numplayers = 1;
    doomcom->numnodes = 1;
    doomcom->deathmatch = false;
    doomcom->consoleplayer = 0;
    doomcom->ticdup = 1;
    doomcom->extratics = 0;
}

void I_NetCmd(void)      {}

/* ============================================================================
 * Input — translate ASCII to DOOM key codes
 * ============================================================================ */
static int translate_key(char c, int *doom_key)
{
    switch (c) {
        case 'w': case 'W': *doom_key = KEY_UPARROW;    return 1;
        case 's': case 'S': *doom_key = KEY_DOWNARROW;  return 1;
        case 'a': case 'A': *doom_key = KEY_LEFTARROW;  return 1;
        case 'd': case 'D': *doom_key = KEY_RIGHTARROW; return 1;
        case 'f': case 'F': *doom_key = KEY_RCTRL;      return 1;  /* fire   */
        case 'e': case 'E': *doom_key = KEY_RSHIFT;     return 1;  /* run    */
        case 'z': case 'Z': *doom_key = ',';            return 1;  /* strafe L */
        case 'x': case 'X': *doom_key = '.';            return 1;  /* strafe R */
        case '\r': case '\n': *doom_key = KEY_ENTER;    return 1;
        case '\033': *doom_key = KEY_ESCAPE;             return 1;
        case ' ':   *doom_key = ' ';                     return 1;  /* use    */
        case '\b':  *doom_key = KEY_BACKSPACE;           return 1;
        case '8': *doom_key = KEY_UPARROW;      return 1;
        case '2': *doom_key = KEY_DOWNARROW;    return 1;
        case '4': *doom_key = KEY_LEFTARROW;    return 1;
        case '6': *doom_key = KEY_RIGHTARROW;   return 1;
        case '5': *doom_key = ' ';              return 1;
        default:
            if (c >= 'a' && c <= 'z') { *doom_key = c; return 1; }
            if (c >= '1' && c <= '9') { *doom_key = c; return 1; }
            if (c == '0')             { *doom_key = '0'; return 1; }
            return 0;
    }
}

void I_StartTic(void)
{
    event_t ev;
    int dk;

    /* --- PS/2 keyboard: proper press AND release events (held keys work) --- */
    int kev;
    while ((kev = kb_getevent()) != 0) {
        char c = (kev > 0) ? (char)kev : (char)(-kev);
        int is_press = (kev > 0);

        /* Quit on Q or Escape press only */
        if (is_press && (c == 'q' || c == 'Q' || c == '\033')) {
            tios_doom_quit_requested = 1;
            doom_running = 0;
            return;
        }

        if (translate_key(c, &dk)) {
            ev.type  = is_press ? ev_keydown : ev_keyup;
            ev.data1 = dk;
            D_PostEvent(&ev);
        }
    }

    /* --- UART fallback (serial / make qemu mode): discrete press+release --- */
    {
        /* Check UART directly */
        #define UART0_BASE  0x101f1000UL
        #define UART_DR_I   (*(volatile unsigned int *)(UART0_BASE + 0x000))
        #define UART_FR_I   (*(volatile unsigned int *)(UART0_BASE + 0x018))
        #define UART_FR_RXFE_I (1 << 4)
        if (!(UART_FR_I & UART_FR_RXFE_I)) {
            char c = (char)(UART_DR_I & 0xFF);
            if (c == 'q' || c == 'Q' || c == '\033') {
                tios_doom_quit_requested = 1;
                doom_running = 0;
                return;
            }
            if (translate_key(c, &dk)) {
                ev.type = ev_keydown; ev.data1 = dk; D_PostEvent(&ev);
                ev.type = ev_keyup;                  D_PostEvent(&ev);
            }
        }
    }
}

/* ============================================================================
 * FAT-backed POSIX file descriptor emulation for W_AddFile / W_ReadLump
 * ============================================================================ */
#define TIOS_MAX_FD 4

typedef struct {
    fat_file_t *ff;
    int         pos;
    int         size;
    int         used;
} tios_fd_t;

static tios_fd_t fd_table[TIOS_MAX_FD];

/* Convert a DOOM path like "doom.wad" → FAT 8.3 uppercase name */
static void path_to_fat(const char *path, char *out)
{
    /* strip leading path separators, take basename */
    const char *p = path;
    for (const char *q = path; *q; q++)
        if (*q == '/' || *q == '\\') p = q + 1;
    int i = 0;
    while (*p && i < 11) { out[i++] = (char)tios_toupper(*p++); }
    out[i] = '\0';
}

int tios_access(const char *pathname, int mode)
{
    (void)mode;
    char fatname[12];
    path_to_fat(pathname, fatname);

    /* We only have DOOM.WAD for now */
    if (strcmp(fatname, "DOOM.WAD") == 0) return 0;
    if (strcmp(fatname, "DOOM1.WAD") == 0) return 0;
    return -1;
}

int tios_open(const char *path, int flags, ...)
{
    (void)flags;
    char fatname[12];
    path_to_fat(path, fatname);

    for (int i = 0; i < TIOS_MAX_FD; i++) {
        if (!fd_table[i].used) {
            fat_file_t *ff = fat_open(fatname);
            if (!ff) { kputs("[DOOM] open failed: "); kputs(fatname); kputc('\n'); return -1; }
            kputs("Troubleshot Dawg:\n check you have placed doom.wad at /games/em-doom/ for qemu, else try command doom gfx -iwad doom1.wad\n");  
            fd_table[i].ff   = ff;
            fd_table[i].pos  = 0;
            fd_table[i].size = (int)fat_file_size(ff);
            fd_table[i].used = 1;
            kputs("[DOOM] opened "); kputs(fatname); kputc('\n');
            return TIOS_FD_OFFSET + i;
        }
    }
    kputs("[DOOM] no free fd\n");
    return -1;
}

int tios_read(int fd, void *buf, int count)
{
    int idx = fd - TIOS_FD_OFFSET;
    if (idx < 0 || idx >= TIOS_MAX_FD || !fd_table[idx].used) return -1;
    tios_fd_t *t = &fd_table[idx];
    int n = fat_read(t->ff, buf, count);
    if (n > 0) t->pos += n;
    return n;
}

int tios_lseek(int fd, int offset, int whence)
{
    int idx = fd - TIOS_FD_OFFSET;
    if (idx < 0 || idx >= TIOS_MAX_FD || !fd_table[idx].used) return -1;
    tios_fd_t *t = &fd_table[idx];
    int target;
    switch (whence) {
        case 0: target = offset; break;                      /* SEEK_SET */
        case 1: target = t->pos + offset; break;            /* SEEK_CUR */
        case 2: target = t->size + offset; break;           /* SEEK_END */
        default: return -1;
    }
    if (target < 0) target = 0;
    /* Seek within file */
    fat_seek(t->ff, target);
    t->pos = target;
    return target;
}

int tios_close(int fd)
{
    int idx = fd - TIOS_FD_OFFSET;
    if (idx < 0 || idx >= TIOS_MAX_FD || !fd_table[idx].used) return -1;
    fat_close(fd_table[idx].ff);
    fd_table[idx].used = 0;
    return 0;
}

int tios_fstat(int fd, void *statbuf)
{
    int idx = fd - TIOS_FD_OFFSET;
    if (idx < 0 || idx >= TIOS_MAX_FD || !fd_table[idx].used) return -1;
    struct { int st_size; int st_mode; int st_uid; } *sb = statbuf;
    sb->st_size = fd_table[idx].size;
    sb->st_mode = 0;
    sb->st_uid  = 0;
    return 0;
}

/* ============================================================================
 * Entry point called from T-OS command.c
 * ============================================================================ */
/* DOOM's real entry */
extern void D_DoomMain(void);

/* argc/argv we pass to DOOM */
static char *doom_argv[4];
static char  doom_wadpath[] = "DOOM.WAD";
static char  doom_nomus[]   = "-nomusic";
static char  doom_nosnd[]   = "-nosound";

/* Declare singletics so we can force it on for bare-metal (avoids the
 * TryRunTics catch-up storm that freezes the game after level load). */
extern boolean singletics;

static void doom_engine_run_wad(const char* wadname)
{
    /* Keep the gfx console alive so all init/debug messages are visible
     * while the WAD loads.  I_InitGraphics() will take over the FB when
     * DOOM is actually ready to render (disabling the console there). */
    kputs("\n");
    kputs("========================================\n");
    kputs("  DOOM Loading — please wait...\n");
    kputs("========================================\n");
    kputs("[DOOM] Initializing em-doom engine\n");

    /* Force singletics: run exactly 1 game tic per rendered frame.
     * Without this, TryRunTics catches up all tics missed during the slow
     * level load (hundreds of tics), causing an apparent freeze. */
    singletics = true;

    /* Reset slab allocator for a fresh run */
    doom_slab_pos = 0;
    tios_doom_quit_requested = 0;

    /* Wipe fd table & close leaked handles from previous run */
    for (int i = 0; i < TIOS_MAX_FD; i++) {
        if (fd_table[i].used) {
            fat_close(fd_table[i].ff);
            fd_table[i].used = 0;
        }
    }

    /* Build argv */
    doom_argv[0] = (char *)"tios-doom";
    doom_argv[1] = (char *)wadname;
    doom_argv[2] = (char *)doom_nomus;
    doom_argv[3] = (char *)doom_nosnd;

    myargc = 4;
    myargv = doom_argv;

    doom_running = 1;

    D_DoomMain();

    /* Restore console after DOOM exits */
    kputs("[DOOM] Engine returned\n");
    fb_clear(0x0000);
    gfx_console_enable(1);
}

void doom_engine_run(void)
{
    doom_engine_run_wad("DOOM.WAD");
}

void doom2_engine_run(void)
{
    doom_engine_run_wad("DOOM2.WAD");
}
