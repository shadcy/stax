#include "arch/cc.h"
#include <stdint.h>
#include <stddef.h>

extern volatile unsigned int tick_count;

/* Returns current time in milliseconds */
u32_t sys_now(void) {
    return tick_count;
}

/* ------------------------------------------------------------------------- *
 *  Missing LibC functions for lwIP (since T-OS doesn't have a full libc)    *
 * ------------------------------------------------------------------------- */


int atoi(const char *str) {
    int res = 0;
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res;
}

/* basic ctype replacements for lwIP if it links against them */
int islower(int c) { return (c >= 'a' && c <= 'z'); }
int isupper(int c) { return (c >= 'A' && c <= 'Z'); }
int isdigit(int c) { return (c >= '0' && c <= '9'); }
int isspace(int c) { return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'); }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int isprint(int c) { return (c >= 0x20 && c <= 0x7E); }
int isalnum(int c) { return islower(c) || isupper(c) || isdigit(c); }
int isalpha(int c) { return islower(c) || isupper(c); }

/* In case of _ctype_ array references from gcc builtins */
const unsigned char _ctype_[256] = {0};

const char *lwip_strerr(int err) {
    return "lwip_err";
}
