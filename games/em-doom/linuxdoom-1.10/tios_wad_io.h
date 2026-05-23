/* ============================================================================
 * tios_wad_io.h — FAT-backed file descriptor emulation for DOOM's WAD I/O
 * ============================================================================ */
#ifndef TIOS_WAD_IO_H
#define TIOS_WAD_IO_H

#include <stdint.h>
#include <stddef.h>

/* Open flags (minimal) */
#define O_RDONLY  0
#define O_BINARY  0
#define SEEK_SET  0
#define SEEK_CUR  1
#define SEEK_END  2

/* File descriptor handle — we only ever open one WAD file */
#define TIOS_MAX_FD 4
#define TIOS_FD_OFFSET 3   /* Start FDs at 3 to skip stdin/stdout/stderr */

int   tios_open(const char *path, int flags, ...);
int   tios_read(int fd, void *buf, int count);
int   tios_lseek(int fd, int offset, int whence);
int   tios_close(int fd);
int   tios_fstat(int fd, void *statbuf);

#define open(...)                 tios_open(__VA_ARGS__)
#define read(fd, buf, count)      tios_read(fd, buf, count)
#define lseek(fd, off, whence)    tios_lseek(fd, off, whence)
#define close(fd)                 tios_close(fd)
#define fstat(fd, sb)             tios_fstat(fd, sb)

/* stat struct that filelength() needs */
struct stat {
    int st_size;
    int st_mode;
    int st_uid;
};

#endif /* TIOS_WAD_IO_H */
