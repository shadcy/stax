/* ============================================================================
 * tios_compat.h — POSIX compatibility shim for linuxdoom on T-OS
 *
 * Force-included by the compiler (-include tios_compat.h) so DOOM sources
 * see T-OS primitives instead of glibc/Linux headers.
 * ============================================================================ */
#ifndef TIOS_COMPAT_H
#define TIOS_COMPAT_H

#define TIOS 1
#define NORMALUNIX 1

/* ---- Prevent ALL POSIX headers from being included ---- */
#define _STDLIB_H 1
#define _STDIO_H  1
#define _STRING_H 1
#define _UNISTD_H 1
#define _FCNTL_H  1
#define _MALLOC_H 1
#define _ALLOCA_H 1
#define _ERRNO_H  1
#define _SIGNAL_H 1
#define _CTYPE_H  1
#define _MATH_H   1
#define _SYS_TIME_H  1
#define _SYS_TYPES_H 1
#define _SYS_STAT_H  1
#define _SYS_IPC_H   1
#define _SYS_SHM_H   1
#define _SYS_SOCKET_H 1
#define _NETINET_IN_H 1
#define _STDARG_H_INCLUDED 1

#define _STDLIB_H_ 1
#define _STDIO_H_  1
#define _STRING_H_ 1
#define _UNISTD_H_ 1
#define _FCNTL_H_  1
#define _MALLOC_H_ 1
#define _ALLOCA_H_ 1
#define _ERRNO_H_  1
#define _SIGNAL_H_ 1
#define _CTYPE_H_  1
#define _MATH_H_   1
#define _SYS_TIME_H_  1
#define _SYS_TYPES_H_ 1
#define _SYS_STAT_H_  1
#define _SYS_IPC_H_   1
#define _SYS_SHM_H_   1
#define _SYS_SOCKET_H_ 1
#define _NETINET_IN_H_ 1
#define _STDARG_H_ 1

#include <stdint.h>
#include <stddef.h>

/* ---- Basic C types ---- */
typedef unsigned char  uint8_t_alt;
typedef unsigned short uint16_t_alt;
typedef unsigned int   uint32_t_alt;
typedef signed char    int8_t_alt;
typedef signed short   int16_t_alt;
typedef signed int     int32_t_alt;
typedef int            mode_t;

/* ---- POSIX file descriptor emulation ---- */
#include "tios_wad_io.h"

/* ---- va_list support ---- */
typedef __builtin_va_list va_list;
#define va_start(v,l)  __builtin_va_start(v,l)
#define va_end(v)      __builtin_va_end(v)
#define va_arg(v,l)    __builtin_va_arg(v,l)

/* ---- File-like types DOOM uses ---- */
typedef struct _FILE FILE;
#define stdout  ((FILE*)0)
#define stderr  ((FILE*)0)
#define fflush(f) ((void)0)
#define fclose(f) ((void)0)
#define fopen(name, mode) ((FILE*)0)

/* ---- String functions ---- */
void  *tios_memset(void *s, int c, size_t n);
void  *tios_memcpy(void *d, const void *s, size_t n);
int    tios_memcmp(const void *s1, const void *s2, size_t n);
size_t tios_strlen(const char *s);
char  *tios_strncpy(char *d, const char *s, size_t n);
int    tios_strncmp(const char *s1, const char *s2, size_t n);
int    tios_strcasecmp(const char *s1, const char *s2);
char  *tios_strchr(const char *s, int c);
int    tios_atoi(const char *s);
int    tios_sprintf(char *buf, const char *fmt, ...);
int    tios_vsprintf(char *buf, const char *fmt, va_list args);

#define memset   tios_memset
#define memcpy   tios_memcpy
#define memcmp   tios_memcmp
#define strlen   tios_strlen
#define strncpy  tios_strncpy
#define strncmp  tios_strncmp
#define strcasecmp tios_strcasecmp
#define strcmpi  tios_strcasecmp
#define strchr   tios_strchr
#define atoi     tios_atoi
#define sprintf  tios_sprintf
#define vsprintf tios_vsprintf
#define fprintf(f, ...) tios_kprintf(__VA_ARGS__)
#define printf(...)     tios_kprintf(__VA_ARGS__)
#define vfprintf(f, fmt, args) tios_vsprintf((char*)0, fmt, args)

/* toupper/tolower */
static inline int tios_toupper(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}
static inline int tios_tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}
#define toupper tios_toupper
#define tolower tios_tolower
#define isspace(c) ((c)==' '||(c)=='\t'||(c)=='\n'||(c)=='\r')
#define isdigit(c) ((c)>='0'&&(c)<='9')
#define isupper(c) ((c)>='A'&&(c)<='Z')
#define islower(c) ((c)>='a'&&(c)<='z')
#define isalpha(c) (isupper(c)||islower(c))

/* ---- Memory allocation ---- */
void *tios_doom_malloc(size_t size);
void  tios_doom_free(void *ptr);
void *tios_doom_realloc(void *ptr, size_t newsize);
void *tios_doom_alloca(size_t size);

#define malloc(n)      tios_doom_malloc(n)
#define free(p)        tios_doom_free(p)
#define realloc(p,n)   tios_doom_realloc(p,n)
#define alloca(n)      tios_doom_alloca(n)

/* ---- Process control ---- */
extern volatile int tios_doom_quit_requested;  /* set to exit D_DoomLoop / I_Error */
static inline void tios_exit(int code) { (void)code; tios_doom_quit_requested = 1; while(1) { __asm__ volatile("nop"); } }
#define exit(c)  tios_exit(c)
#define abort()  tios_exit(-1)

/* ---- Signal stubs ---- */
#define SIG_DFL ((void(*)(int))0)
#define SIGINT  2
static inline void *tios_signal(int sig, void *handler) { (void)sig; (void)handler; return (void*)0; }
#define signal(s,h) tios_signal(s,h)

/* ---- errno ---- */
extern int tios_errno;
#define errno tios_errno
#define ENOENT 2

/* ---- Math (integers only, no floats in DOOM logic) ---- */
static inline int tios_abs(int x) { return x < 0 ? -x : x; }
#define abs(x) tios_abs(x)

/* ---- usleep / sleep ---- */
static inline void tios_usleep(unsigned int us) {
    volatile unsigned int n = us * 20;
    while (n--) __asm__ volatile("nop");
}
#define usleep(n) tios_usleep(n)
#define sleep(n)  tios_usleep((n)*1000000)

/* ---- getenv ---- */
static inline char *tios_getenv(const char *name) { (void)name; return (char*)0; }
#define getenv(n) tios_getenv(n)

/* ---- uid ---- */
static inline int tios_getuid(void) { return 0; }
#define getuid() tios_getuid()

/* ---- values.h emulation ---- */
#define _VALUES_H 1
#define MININT (0x80000000)
#define MAXINT (0x7FFFFFFF)

/* ---- access / mkdir / write ---- */
#define R_OK 4
int tios_access(const char *pathname, int mode);
#define access(p, m) tios_access(p, m)
#define mkdir(p, m) (-1)
#define write(fd, buf, count) (-1)

/* ---- stdio additions ---- */
#define setbuf(f, b) ((void)0)
extern char kgetc(void);
static inline int tios_getchar(void) { return (int)kgetc(); }
#define getchar() tios_getchar()

/* ---- DOOM's own printf helper ---- */
void tios_kprintf(const char *fmt, ...);

#endif /* TIOS_COMPAT_H */
