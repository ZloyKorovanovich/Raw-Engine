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

#define I32_MAX (2147483647)

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

#define EXPORT __declspec(dllexport)

typedef struct {
    u32   element_size;
    u32   capacity;
    u32   free_count;
    u32   growth;
    void* pool;
    u32*  free_slots;
} Pool;

b32 pool_create(Pool* pool, u32 element_size, u32 growth, u32 capacity);
void* pool_add(Pool* pool);
u32 pool_add_id(Pool* pool);
void pool_remove(Pool* pool, void* element);
void pool_remove_id(Pool* pool, u32 id);
void pool_destroy(Pool* pool);

typedef struct {
    u32   element_size;
    u32   capacity;
    u32   count;
    u32   growth;
    void* array;
} Array;

b32 array_create(Array* array, u32 element_size, u32 growth, u32 capacity);
void* array_add(Array* array);
void array_remove(Array* array, void* element);
void array_remove_id(Array* array, u32 id);
void array_destroy(Array* array);

#endif