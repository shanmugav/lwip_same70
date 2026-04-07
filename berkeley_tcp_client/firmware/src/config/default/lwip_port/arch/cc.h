/* lwIP compiler/platform abstraction for SAM E70 with XC32 (GCC) */
#ifndef CC_H_INCLUDED
#define CC_H_INCLUDED

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

typedef uint8_t            u8_t;
typedef int8_t             s8_t;
typedef uint16_t           u16_t;
typedef int16_t            s16_t;
typedef uint32_t           u32_t;
typedef int32_t            s32_t;
typedef uintptr_t          mem_ptr_t;

#define U16_F "hu"
#define S16_F "hd"
#define X16_F "hx"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"

#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT __attribute__ ((packed))
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

#define LWIP_COMPAT_MUTEX  1
#define LWIP_PROVIDE_ERRNO

#define LWIP_PLATFORM_DIAG(x)   { ; }
#define LWIP_PLATFORM_ASSERT(x) { while(1); }

#endif /* CC_H_INCLUDED */
