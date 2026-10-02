#include "game.h"
#include "game_structs.h"
#include "../input/input.h"

extern b32  graphics_init(GpuContext* gpu_ctx);
extern void graphics_terminate(void);

static GpuContext* gpu_ctx = NULL; 

/* === entity pool === */
static u64 global_entity_id = 1;
static Pool entity_pool = (Pool){.element_size = sizeof(Entity), .growth = 1024};

/* just zeroed entity with id set */
Entity* entity_create(const char* name) {
    Entity* entity = pool_add(&entity_pool);
    if(entity == NULL) {
        return NULL;
    } else {
        *entity = (Entity) {
            .entity_id = global_entity_id++,
            .scale     = 1.0,
            .position  = (Vec3){0},
            .rotation  = (Vec4){0.0, 0.0, 0.0, 1.0}
        };
        strcpy_s(entity->name, PATH_LENGTH, name);
        return entity;
    }
}

void entity_destroy(Entity* entity) {
    if(entity != NULL && entity->entity_id != INVALID_ENTITY_ID) {
        pool_remove(&entity_pool, entity);
    }
}

b32 engine_init(b32 is_debug) {
    const GpuInitInfo gpu_init_info = {
        .window_name       = "demo",
        .window_width      = 800,
        .window_height     = 600,
        .config_flags      = is_debug ? GPU_CONFIG_DEBUG | GPU_CONFIG_VSYNC : GPU_CONFIG_VSYNC,
        .images_heap_size  = 1 * GB,
        .malloc_heap_size  = 1 * GB,
        .images_max_count  = 1024,
        .pci_vendor_device = GPU_PCI_ANY  
    };

    gpu_ctx = gpu_init(&gpu_init_info);
    if(gpu_ctx == NULL) {
        LOG_ERROR("failed to init gpu");
        goto fail;
    }
    if(!graphics_init(gpu_ctx)) {
        LOG_ERROR("failed to init engine");
        goto fail;
    }

    /* gpu_ctx hooks */
    input_hook_window(gpu_get_glfw_window(gpu_ctx));
    resources_hook_gpu(gpu_ctx);

    return TRUE;

    fail: {
        return FALSE;
    }
}

void engine_terminate(void) {
    /* entity pool */ {
        pool_destroy(&entity_pool);
        global_entity_id = 0;
        entity_pool      = (Pool){0};
    }
    
    graphics_terminate();
    mesh_free_all();
    texture_free_all();
    gpu_terminate(gpu_ctx);
    gpu_ctx          = NULL;
}
