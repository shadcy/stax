/* ============================================================================
 * stax_wad_io.h — FAT-backed file descriptor emulation for DOOM's WAD I/O
 * ============================================================================ */
#ifndef STAX_WAD_IO_H
#define STAX_WAD_IO_H

#include <stdint.h>
#include <stddef.h>

/* Open flags (minimal) */
#define O_RDONLY  0
#define O_BINARY  0
#define SEEK_SET  0
#define SEEK_CUR  1
#define SEEK_END  2

/* File descriptor handle — we only ever open one WAD file */
#define STAX_MAX_FD 4
#define STAX_FD_OFFSET 3   /* Start FDs at 3 to skip stdin/stdout/stderr */

int   stax_open(const char *path, int flags, ...);
int   stax_read(int fd, void *buf, int count);
int   stax_lseek(int fd, int offset, int whence);
int   stax_close(int fd);
int   stax_fstat(int fd, void *statbuf);

#define open(...)                 stax_open(__VA_ARGS__)
#define read(fd, buf, count)      stax_read(fd, buf, count)
#define lseek(fd, off, whence)    stax_lseek(fd, off, whence)
#define close(fd)                 stax_close(fd)
#define fstat(fd, sb)             stax_fstat(fd, sb)

/* stat struct that filelength() needs */
struct stat {
    int st_size;
    int st_mode;
    int st_uid;
};

#endif /* STAX_WAD_IO_H */
