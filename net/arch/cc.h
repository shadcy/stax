#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>

typedef uint8_t  u8_t;
typedef int8_t   s8_t;
typedef uint16_t u16_t;
typedef int16_t  s16_t;
typedef uint32_t u32_t;
typedef int32_t  s32_t;

#define U16_F "hu"
#define S16_F "hd"
#define X16_F "hx"
#define U32_F "lu"
#define S32_F "ld"
#define X32_F "lx"

#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

/* Must vary — constant RAND breaks DNS TXID / any future RAND_SRC_PORT use. */
extern volatile unsigned int tick_count;
#define LWIP_RAND() ((u32_t)(tick_count * 2654435761u + 0x9E3779B9u))

#define LWIP_NO_CTYPE_H 1

#endif /* LWIP_ARCH_CC_H */
