#ifndef _RESOURCES_STRUCTS_INCLUDED
#define _RESOURCES_STRUCTS_INCLUDED

#include "resources.h"

typedef union {
    char name[PATH_LENGTH];
    u64  key [PATH_LENGTH / sizeof(u64)];
} ResourceKey;

#endif
