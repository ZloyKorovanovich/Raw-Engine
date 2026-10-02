#include "base.h"

/* === pool === */

b32 pool_create(Pool* pool, u32 element_size, u32 growth, u32 capacity) {
    void* pool_array  = NULL;
    u32*  free_slots = NULL;

    if(capacity != 0) {
        pool_array = calloc(capacity, element_size);
        free_slots = calloc(capacity, sizeof(u32));

        if(pool_array == NULL || free_slots == NULL) {
            goto fail;
        }

        for(u32 i = 0; i != capacity; i++) {
            free_slots[i] = i;
        }
    }

    *pool = (Pool) {
        .capacity     = capacity,
        .element_size = element_size,
        .free_count   = capacity,
        .growth       = growth,
        .pool         = pool_array,
        .free_slots   = free_slots
    };
    return TRUE;

    fail: {
        free(free_slots);
        free(pool_array);
        return FALSE;
    }
}

void* pool_add(Pool* pool) {
    const u32 growth       = pool->growth;
    const u32 element_size = pool->element_size;
    const u32 old_capacity = pool->capacity;
    const u32 old_free_count = pool->free_count;

    if(old_free_count == 0) {
        if(pool->growth == 0) {
            goto fail;
        }

        u32   new_capacity   = old_capacity + growth;
        u32   new_free_count = old_free_count + growth;
        void* new_pool_array = realloc(pool->pool      , (u64)new_capacity * (u64)element_size);
        u32*  new_free_slots = realloc(pool->free_slots, (u64)new_capacity * sizeof(u32));

        pool->pool       = new_pool_array == NULL ? pool->pool       : new_pool_array;
        pool->free_slots = new_free_slots == NULL ? pool->free_slots : new_free_slots;

        if(new_pool_array == NULL || new_free_slots == NULL) {
            goto fail;
        }

        pool->capacity = new_capacity;
        pool->free_count = new_free_count;

        memset((u8*)pool->pool       + old_capacity * element_size, 0, (new_capacity - old_capacity) * element_size);
        memset((u8*)pool->free_slots + old_capacity * sizeof(u32) , 0, (new_capacity - old_capacity) * sizeof(u32));
        for(u32 i = 0; i != growth; i++) {
            new_free_slots[old_free_count + i] = old_capacity + i;
        }
    }

    const u32 free_slots_count = pool->free_count;
    const u32 element_id       = pool->free_slots[free_slots_count - 1];
    pool->free_count--;

    return (u8*)pool->pool + (u64)element_id * (u64)element_size;

    fail: {
        return NULL;
    }
}

u32 pool_add_id(Pool* pool) {
    void* slot = pool_add(pool);
    if(slot == NULL) {
        return U32_MAX;
    } else {
        return (u32)(((u64)slot - (u64)pool->pool) / (u64)pool->element_size);
    }
}


void pool_remove(Pool* pool, void* element) {
    if(pool->capacity == 0) {
        goto fail;
    }
    if((u64)pool->pool > (u64)element) {
        goto fail;
    }

    const u32 free_count   = pool->free_count;
    const u32 element_size = pool->element_size;
    const u32 element_id   = (u32)((u64)element - (u64)pool->pool) / (u64)element_size;

    if(element_id >= pool->capacity) {
        goto fail;
    }

    memset(element, 0, element_size);
    pool->free_slots[free_count] = element_id;
    pool->free_count++;
    
    fail: {}
}

void pool_remove_id(Pool* pool, u32 id) {
    pool_remove(pool, (u8*)pool->pool + (u64)id * (u64)pool->element_size);
}

void pool_destroy(Pool* pool) {
    free(pool->pool);
    free(pool->free_slots);
    *pool = (Pool){0};
}

/* === array === */

b32 array_create(Array* array, u32 element_size, u32 growth, u32 capacity) {
    void* allocation = NULL;
    if(capacity != 0) {
        allocation = calloc(capacity, element_size);
        if(allocation == NULL) {
            LOG_ERROR("failed to allocate array");
            goto fail;
        }
    }

    *array = (Array) {
        .element_size = element_size,
        .capacity     = capacity,
        .growth       = growth,
        .array        = allocation
    };
    return TRUE;

    fail: {
        return FALSE;
    }
}

void* array_add(Array* array) {
    const u32 old_count    = array->count;
    const u32 old_capacity = array->capacity;
    const u32 growth       = array->growth;
    const u32 element_size = array->element_size;

    if(old_count + 1 > old_capacity) {
        if(growth == 0) {
            goto fail;
        }

        u32   new_capacity   = old_capacity + growth;
        void* new_allocation = realloc(array->array, (u64)new_capacity * (u64)element_size);
        if(new_allocation == NULL) {
            goto fail;
        }

        array->capacity = new_capacity;
        array->array = new_allocation;
    }

    array->count++;
    return (u8*)array->array + (u64)old_count * (u64)element_size;

    fail: {
        return NULL;
    }
}

void array_remove(Array* array, void* element) {
    const u32 old_count    = array->count;
    const u32 element_size = array->element_size;
    void* array_buffer = array->array;

    if((u64)element < (u64)array_buffer) {
        goto fail;
    }
    if((u64)element + (u64)element_size - (u64)array_buffer > (u64)old_count * (u64)element_size) {
        goto fail;
    }

    void* last_element = (u8*)array_buffer + (old_count - 1) * element_size;
    memcpy(element, last_element, element_size);
    memset(last_element, 0, element_size);
    array->count--;

    fail: {}
}

void array_remove_id(Array* array, u32 id) {
    array_remove(array, (u8*)array->array + ((u64)id * (u64)array->element_size));
}

void array_destroy(Array* array) {
    free(array->array);
    *array = (Array){0};
}
