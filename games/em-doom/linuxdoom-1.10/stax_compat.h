/* ============================================================================
 * stax_compat.h — POSIX compatibility shim for linuxdoom on STAX
 *
 * Force-included by the compiler (-include stax_compat.h) so DOOM sources
 * see STAX primitives instead of glibc/Linux headers.
 * ============================================================================ */
#ifndef STAX_COMPAT_H
#define STAX_COMPAT_H

#define STAX 1
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
#include "stax_wad_io.h"

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
void  *stax_memset(void *s, int c, size_t n);
void  *stax_memcpy(void *d, const void *s, size_t n);
int    stax_memcmp(const void *s1, const void *s2, size_t n);
size_t stax_strlen(const char *s);
char  *stax_strncpy(char *d, const char *s, size_t n);
int    stax_strncmp(const char *s1, const char *s2, size_t n);
int    stax_strcasecmp(const char *s1, const char *s2);
char  *stax_strchr(const char *s, int c);
int    stax_atoi(const char *s);
int    stax_sprintf(char *buf, const char *fmt, ...);
int    stax_vsprintf(char *buf, const char *fmt, va_list args);

#define memset   stax_memset
#define memcpy   stax_memcpy
#define memcmp   stax_memcmp
#define strlen   stax_strlen
#define strncpy  stax_strncpy
#define strncmp  stax_strncmp
#define strcasecmp stax_strcasecmp
#define strcmpi  stax_strcasecmp
#define strchr   stax_strchr
#define atoi     stax_atoi
#define sprintf  stax_sprintf
#define vsprintf stax_vsprintf
#define fprintf(f, ...) stax_kprintf(__VA_ARGS__)
#define printf(...)     stax_kprintf(__VA_ARGS__)
#define vfprintf(f, fmt, args) stax_vsprintf((char*)0, fmt, args)

/* toupper/tolower */
static inline int stax_toupper(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}
static inline int stax_tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}
#define toupper stax_toupper
#define tolower stax_tolower
#define isspace(c) ((c)==' '||(c)=='\t'||(c)=='\n'||(c)=='\r')
#define isdigit(c) ((c)>='0'&&(c)<='9')
#define isupper(c) ((c)>='A'&&(c)<='Z')
#define islower(c) ((c)>='a'&&(c)<='z')
#define isalpha(c) (isupper(c)||islower(c))

/* ---- Memory allocation ---- */
void *stax_doom_malloc(size_t size);
void  stax_doom_free(void *ptr);
void *stax_doom_realloc(void *ptr, size_t newsize);
void *stax_doom_alloca(size_t size);

#define malloc(n)      stax_doom_malloc(n)
#define free(p)        stax_doom_free(p)
#define realloc(p,n)   stax_doom_realloc(p,n)
#define alloca(n)      stax_doom_alloca(n)

/* ---- Process control ---- */
extern volatile int stax_doom_quit_requested;  /* set to exit D_DoomLoop / I_Error */
static inline void stax_exit(int code) { (void)code; stax_doom_quit_requested = 1; while(1) { __asm__ volatile("nop"); } }
#define exit(c)  stax_exit(c)
#define abort()  stax_exit(-1)

/* ---- Signal stubs ---- */
#define SIG_DFL ((void(*)(int))0)
#define SIGINT  2
static inline void *stax_signal(int sig, void *handler) { (void)sig; (void)handler; return (void*)0; }
#define signal(s,h) stax_signal(s,h)

/* ---- errno ---- */
extern int stax_errno;
#define errno stax_errno
#define ENOENT 2

/* ---- Math (integers only, no floats in DOOM logic) ---- */
static inline int stax_abs(int x) { return x < 0 ? -x : x; }
#define abs(x) stax_abs(x)

/* ---- usleep / sleep ---- */
static inline void stax_usleep(unsigned int us) {
    volatile unsigned int n = us * 20;
    while (n--) __asm__ volatile("nop");
}
#define usleep(n) stax_usleep(n)
#define sleep(n)  stax_usleep((n)*1000000)

/* ---- getenv ---- */
static inline char *stax_getenv(const char *name) { (void)name; return (char*)0; }
#define getenv(n) stax_getenv(n)

/* ---- uid ---- */
static inline int stax_getuid(void) { return 0; }
#define getuid() stax_getuid()

/* ---- values.h emulation ---- */
#define _VALUES_H 1
#define MININT (0x80000000)
#define MAXINT (0x7FFFFFFF)

/* ---- access / mkdir / write ---- */
#define R_OK 4
int stax_access(const char *pathname, int mode);
#define access(p, m) stax_access(p, m)
#define mkdir(p, m) (-1)
#define write(fd, buf, count) (-1)

/* ---- stdio additions ---- */
#define setbuf(f, b) ((void)0)
extern char kgetc(void);
static inline int stax_getchar(void) { return (int)kgetc(); }
#define getchar() stax_getchar()

/* ---- DOOM's own printf helper ---- */
void stax_kprintf(const char *fmt, ...);

#endif /* STAX_COMPAT_H */
