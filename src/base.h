#ifndef _BASE_INCLUDED
#define _BASE_INCLUDED

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef char               i8;
typedef short              i16;
typedef int                i32;
typedef long long int      i64;

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned           u32;
typedef unsigned long long u64;

typedef int                b32;

typedef float              f32;
typedef double             f64;

#define U16_MAX (0xFFFF)
#define U32_MAX (0xFFFFFFFF)
#define U64_MAX (0xFFFFFFFFFFFFFFFFllu)

#define KB (1024llu)
#define MB (1024llu * 1024)
#define GB (1024llu * 1024 * 1024)

#define MIN(a, b)      (a < b ? a : b)
#define MAX(a, b)      (a > b ? a : b)
#define CLAMP(a, b, t) (t < a ? a : (t > b ? b : t))

#define STRINGIFY(x)                #x
#define TOSTRING(x)                 STRINGIFY(x)
#define DEBUG_TRACE                 " *** " __FILE__ ":" TOSTRING(__LINE__)
#define LOG_MESSAGE(log, ...)       printf(":: " log             "\r\n" __VA_OPT__(,) __VA_ARGS__)
#define LOG_ERROR(log, ...)         printf(":! " log DEBUG_TRACE "\r\n" __VA_OPT__(,) __VA_ARGS__)
#define LOG_WARNING(log, ...)       printf(":? " log DEBUG_TRACE "\r\n" __VA_OPT__(,) __VA_ARGS__)
#define LOG_MESSAGE_TRACE(log, ...) printf(":: " log DEBUG_TRACE "\r\n" __VA_OPT__(,) __VA_ARGS__)
#define TRACE                       printf(":> " DEBUG_TRACE "\r\n");

#define TRUE  (1)
#define FALSE (0)

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#define BYTES_OFFSET(alloc, offset) (void*)((u8*)alloc + offset)

#define PATH_LENGTH (256)
#define ALIGN(p, a) (((u64)p + (u64)a - 1) & ~((u64)a - 1))
#define ALIGN_DOWN(p, a) (((u64)p) & ~((u64)a - 1))
#define IS_ALIGNED(p, a) (!((u64)p & ((u64)a - 1)))

#endif