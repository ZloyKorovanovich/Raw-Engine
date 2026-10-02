#include "game.h"
#include "game_structs.h"
#include "../gpu/gpu.h"

#define INVALID_MESH (INVALID_RESOURCE)
#define INVALID_MATERIAL_ID (U32_MAX)

typedef struct {
    Entity*        entity;
    u64            entity_id;
    u32            material_id;
    MeshHandle     mesh_handle;
    TextureHandle  texture_handle;
    GpuImageHandle gpu_image;
    const GpuMesh* gpu_mesh;
} RenderEntity;

typedef struct {
    const GpuMesh* gpu_mesh;
    GpuImageHandle gpu_image;
    u32            material_id;
} RenderId;

typedef struct {
    Mat4x4 camera_vp;
    Mat4x4 camera_iv;
    Vec4   screen_params;
    Vec4   sun_dir;
    Vec4   sun_color;
} UniformBuffer;

typedef struct {
    Mat4x4 matrix_m;
} OpaqueLitMaterial;

typedef struct {
    Vec4 left;
    Vec4 right;
    Vec4 top;
    Vec4 bottom;
    Vec4 near;
    Vec4 far;
} Frustrum;

/* === global gpu context === */

enum PipelinesIds {
    PIPELINE_DEMO_ID,
    PIPELINE_BLIT_ID,
    PIPELINE_AXIS_ID,
    PIPELINE_PLANET_ID,
    PIPELINE_DEFAULT_ID,
    PIPELINE_COUNT
};

static GpuContext* gpu_ctx = NULL;

static u64 uniform_buffer_address = GPU_INVALID_ADDRESS;

static GpuImageHandle screen_color_handle = GPU_INVALID_HANDLE;
static GpuImageHandle screen_depth_handle = GPU_INVALID_HANDLE;
static u32            screen_width        = 0;
static u32            screen_height       = 0;

b32 compile_pipelines(void) {
    const GpuPipelineInfo pipelines_infos[PIPELINE_COUNT] = {
        [PIPELINE_DEMO_ID] = (GpuPipelineInfo) {
            .flags               = 0,
            .vertex_shader       = "./out/data/demo_v.spv",
            .fragment_shader     = "./out/data/demo_f.spv",
            .color_formats_count = 1,
            .color_formats       = (GpuFormat[]){GPU_FORMAT_R16G16B16A16_SFLOAT},
            .ms_count            = 4  
        },
        [PIPELINE_BLIT_ID] = (GpuPipelineInfo) {
            .flags               = 0,
            .vertex_shader       = "./out/data/blit_msaa_v.spv",
            .fragment_shader     = "./out/data/blit_msaa_f.spv",
            .color_formats_count = 1,
            .color_formats       = (GpuFormat[]){GPU_FORMAT_SURFACE}
        },
        [PIPELINE_AXIS_ID] = (GpuPipelineInfo) {
            .flags               = 0,
            .vertex_shader       = "./out/data/axis_v.spv",
            .fragment_shader     = "./out/data/axis_f.spv",
            .color_formats_count = 1,
            .color_formats       = (GpuFormat[]){GPU_FORMAT_R16G16B16A16_SFLOAT},
            .depth_format        = GPU_FORMAT_D32_SFLOAT,
            .ms_count            = 4  
        },
        [PIPELINE_PLANET_ID] = (GpuPipelineInfo) {
            .flags               = 0,
            .vertex_shader       = "./out/data/planet_v.spv",
            .fragment_shader     = "./out/data/planet_f.spv",
            .color_formats_count = 1,
            .color_formats       = (GpuFormat[]){GPU_FORMAT_R16G16B16A16_SFLOAT},
            .depth_format        = GPU_FORMAT_D32_SFLOAT,
            .ms_count            = 4     
        },
        [PIPELINE_DEFAULT_ID] = (GpuPipelineInfo) {
            .flags               = GPU_PIPELINE_FLAG_CULL_BACK,
            .vertex_shader       = "./out/data/default_v.spv",
            .fragment_shader     = "./out/data/default_f.spv",
            .color_formats_count = 1,
            .color_formats       = (GpuFormat[]){GPU_FORMAT_R16G16B16A16_SFLOAT},
            .depth_format        = GPU_FORMAT_D32_SFLOAT,
            .ms_count            = 4
        }
    };

    if(!gpu_compile_pipelines(gpu_ctx, pipelines_infos, PIPELINE_COUNT)) {
        LOG_ERROR("failed to compile pipelines");
        goto fail;
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

b32 create_uniform_buffer(void) {
    uniform_buffer_address = gpu_malloc(gpu_ctx, sizeof(UniformBuffer), 16);
    if(uniform_buffer_address == GPU_INVALID_ADDRESS) {
        LOG_ERROR("failed to allocate uniform buffer");
        goto fail;
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

b32 resize_screen_textures(void) {
    if(screen_width == 0 || screen_height == 0) {
        goto success;
    }

    /* destroy old images */
    if(screen_color_handle != GPU_INVALID_HANDLE) {
        gpu_remove_image(gpu_ctx, screen_color_handle);
    }
    if(screen_depth_handle != GPU_INVALID_HANDLE) {
        gpu_remove_image(gpu_ctx, screen_depth_handle);
    }

    /* images config */
    const GpuImageInfo screen_color_info = {
        .flags     = GPU_IMAGE_FLAG_COLOR_ATTACHMENT | GPU_IMAGE_FLAG_SAMPLED,
        .format    = GPU_FORMAT_R16G16B16A16_SFLOAT,
        .width     = screen_width,
        .height    = screen_height,
        .mip_count = 1,
        .ms_count  = 4
    };
    const GpuImageInfo screen_depth_info = {
        .flags     = GPU_IMAGE_FLAG_DEPTH_ATTACHMENT | GPU_IMAGE_FLAG_SAMPLED,
        .format    = GPU_FORMAT_D32_SFLOAT,
        .width     = screen_width,
        .height    = screen_height,
        .mip_count = 1,
        .ms_count  = 4
    };

    /* create new images */
    screen_color_handle = gpu_add_image(gpu_ctx, &screen_color_info);
    if(screen_color_handle == GPU_INVALID_HANDLE) {
        LOG_ERROR("failed to create screen color image");
        goto fail;
    }
    screen_depth_handle = gpu_add_image(gpu_ctx, &screen_depth_info);
    if(screen_depth_handle == GPU_INVALID_HANDLE) {
        LOG_ERROR("failed to create screen depth image");
        goto fail;
    }

    success: {
        return TRUE;
    }
    fail: {
        return FALSE;
    }
}

/* returns TRUE if displayed FALSE if culled */
b32 frustrum_culling(const Frustrum* frustrum, Vec3 position, f32 radius) {
    return( signed_distance_from_plane(frustrum->left  , position) > -radius &&
            signed_distance_from_plane(frustrum->right , position) > -radius &&
            signed_distance_from_plane(frustrum->top   , position) > -radius &&
            signed_distance_from_plane(frustrum->bottom, position) > -radius &&
            signed_distance_from_plane(frustrum->near  , position) > -radius &&
            signed_distance_from_plane(frustrum->far   , position) > -radius   );
}

/* === opaque lit === */
#define OPAQUE_LIT_GPU_COUNT (1024)

static u64   opaque_lit_gpu_adderss    = GPU_INVALID_ADDRESS;
static Pool  opaque_lit_cpu_pool       = (Pool){0};
static Array opaque_lit_entities_array = (Array){.element_size = sizeof(RenderEntity), .growth = 1024};

b32 opaque_lit_init(void) {
    opaque_lit_gpu_adderss = gpu_malloc(gpu_ctx, OPAQUE_LIT_GPU_COUNT * sizeof(OpaqueLitMaterial), 16);
    if(opaque_lit_gpu_adderss == GPU_INVALID_ADDRESS) {
        LOG_ERROR("failed to allocate opque lit gpu buffer");
        goto fail;
    }

    if(!pool_create(&opaque_lit_cpu_pool, sizeof(OpaqueLitMaterial), 0, OPAQUE_LIT_GPU_COUNT)) {
        LOG_ERROR("failed to create opaque lit cpu pool");
        goto fail;
    }

    return TRUE;

    fail: {
        gpu_free(gpu_ctx, opaque_lit_gpu_adderss, OPAQUE_LIT_GPU_COUNT * sizeof(OpaqueLitMaterial));
        pool_destroy(&opaque_lit_cpu_pool);
        return FALSE;
    }
}

void opaque_lit_terminate(void) {
    gpu_free(gpu_ctx, opaque_lit_gpu_adderss, OPAQUE_LIT_GPU_COUNT * sizeof(OpaqueLitMaterial));
    pool_destroy(&opaque_lit_cpu_pool);
}

void opaque_lit_remove_by_id(u32 id) {
    if(id >= opaque_lit_entities_array.count) {
        LOG_ERROR("invalid opaque lit array id");
        return;
    }

    RenderEntity* render_entity = &((RenderEntity*)opaque_lit_entities_array.array)[id];
    const GpuMesh*       gpu_mesh       = render_entity->gpu_mesh;
    const GpuImageHandle gpu_image      = render_entity->gpu_image;
    u32                  material_id    = render_entity->material_id;
    MeshHandle           mesh_handle    = render_entity->mesh_handle;
    TextureHandle        texture_handle = render_entity->texture_handle;

    if(material_id != INVALID_MATERIAL_ID) {
        pool_remove_id(&opaque_lit_cpu_pool, material_id);
    }
    if(gpu_mesh != NULL) {
        mesh_unload_gpu(mesh_handle);
    }
    if(gpu_image != GPU_INVALID_HANDLE) {
        texture_unload_gpu(texture_handle);
    }

    array_remove(&opaque_lit_entities_array, render_entity);
}

u32 opaque_lit_render_list(const Frustrum* frustrum, RenderId* render_list, u32 render_list_max_count) {
    u32 render_list_count           = 0;
    u32 materials_begin_id = U32_MAX;
    u32 materials_end_id   = 0;

    for(u32 i = 0; i != opaque_lit_entities_array.count; i++) {
        /* record untill exceed list capacity */
        if(render_list_count == render_list_max_count) {
            break;
        }

        RenderEntity* render_entity = &((RenderEntity*)opaque_lit_entities_array.array)[i];
        Entity* entity    = render_entity->entity;
        u64     entity_id = render_entity->entity_id;

        /* missing entity */
        if(entity == NULL || entity->entity_id != entity_id) {
            opaque_lit_remove_by_id(i--);
            continue;
        }

        const GpuMesh* gpu_mesh       = render_entity->gpu_mesh;
        GpuImageHandle gpu_image      = render_entity->gpu_image; 
        u32            material_id    = render_entity->material_id;
        MeshHandle     mesh_handle    = render_entity->mesh_handle;
        TextureHandle  texture_handle = render_entity->texture_handle;
        b32            is_updated   = entity->is_updated;
        /* reset update state */
        entity->is_updated = FALSE;

        /* in frustrum */
        if(frustrum_culling(frustrum, entity->position, mesh_get_bounding_radius(mesh_handle))) {
            if(material_id == INVALID_MATERIAL_ID) {
                is_updated  = TRUE;
                material_id = pool_add_id(&opaque_lit_cpu_pool);
                if(material_id == INVALID_MATERIAL_ID) {
                    continue;
                }

                materials_begin_id = MIN(materials_begin_id, material_id);
                materials_end_id   = MAX(materials_end_id,   material_id + 1);
            }

            if(gpu_mesh == NULL) {
                gpu_mesh = mesh_load_gpu(mesh_handle);
                if(gpu_mesh == NULL) {
                    LOG_ERROR("failed to load gpu mesh");
                    continue;
                }
            }

            if(gpu_image == GPU_INVALID_HANDLE) {
                gpu_image = texture_load_gpu(texture_handle);
                if(gpu_image == GPU_INVALID_HANDLE) {
                    LOG_ERROR("failed to load gpu image");
                    continue;
                }
            }

            if(is_updated) {
                ((OpaqueLitMaterial*)opaque_lit_cpu_pool.pool)[material_id] = (OpaqueLitMaterial) {
                    .matrix_m = mat4x4_mul_mat4x4(mat4x4_translation(entity->position), mat4x4_rotation(entity->rotation))
                };
            }

            render_list[render_list_count++] = (RenderId) {
                .gpu_mesh    = gpu_mesh,
                .gpu_image   = gpu_image,
                .material_id = material_id
            };
        } 
        /* not in frustrum */
        else {
            if(material_id != INVALID_MATERIAL_ID) {
                pool_remove_id(&opaque_lit_cpu_pool, material_id);
                material_id = INVALID_MATERIAL_ID;
            }
            if(gpu_mesh != NULL) {
                mesh_unload_gpu(mesh_handle);
                gpu_mesh = NULL;
            }
            if(gpu_image != GPU_INVALID_HANDLE) {
                texture_unload_gpu(texture_handle);
                gpu_image = GPU_INVALID_HANDLE;
            }
        }

        render_entity->gpu_mesh     = gpu_mesh;
        render_entity->gpu_image    = gpu_image;
        render_entity->material_id  = material_id;
    }

    /* transfer materials */
    if(materials_begin_id < materials_end_id) {
        u64 offset = materials_begin_id * sizeof(OpaqueLitMaterial);
        u64 size   = materials_end_id * sizeof(OpaqueLitMaterial) - offset;
        gpu_cmd_sync_memwrite(gpu_ctx, (u8*)opaque_lit_cpu_pool.pool + offset, size, opaque_lit_gpu_adderss + offset);
    }

    return render_list_count;
}

b32 graphics_opaque_lit_assign(Entity* entity, MeshHandle mesh, TextureHandle texture) {
    /* validate */
    if(entity == NULL) {
        LOG_ERROR("null entity");
        goto fail;
    }
    if(entity->entity_id == INVALID_ENTITY_ID) {
        LOG_ERROR("invalid entity id");
        goto fail;
    }
    if(mesh == INVALID_MESH) {
        LOG_ERROR("invalid mesh handle");
        goto fail;
    }

    /* add new reference to array */
    RenderEntity* render_entity = array_add(&opaque_lit_entities_array);
    if(render_entity == NULL) {
        LOG_ERROR("failed to add opaque lit entity to array");
        goto fail;
    }

    /* fill reference struct */
    *render_entity = (RenderEntity) {
        .entity         = entity,
        .entity_id      = entity->entity_id,
        .material_id    = INVALID_MATERIAL_ID,
        .gpu_image      = GPU_INVALID_HANDLE,
        .gpu_mesh       = NULL,
        .mesh_handle    = mesh,
        .texture_handle = texture
    };
    return TRUE;

    fail: {
        return FALSE;
    }
}

void graphics_opaque_lit_withdraw(Entity* entity) {
    if(entity == NULL) {
        return;
    }
    
    for(u32 i = 0; i != opaque_lit_entities_array.count; i++) {
        RenderEntity* render_entity = &((RenderEntity*)opaque_lit_entities_array.array)[i];
        if(render_entity->entity == entity && render_entity->entity_id == entity->entity_id) {
            opaque_lit_remove_by_id(i);
            return;
        }
    }
}

/* === interface === */

b32 graphics_init(GpuContext* context) {
    if(context == NULL) {
        LOG_ERROR("invalid gpu context");
        goto fail;
    }
    gpu_ctx = context;

    if(!compile_pipelines()) {
        LOG_ERROR("failed to compile pipelines");
        goto fail;
    }
    if(!create_uniform_buffer()) {
        LOG_ERROR("failed to create uniform buffer");
        goto fail;
    }

    /* create render passes etc */
    if(!opaque_lit_init()) {
        LOG_ERROR("failed to init opaque lit");
        goto fail;
    }

    return TRUE;
    
    fail: {
        return FALSE;
    }
}

void graphics_terminate(void) {
    opaque_lit_terminate();
    gpu_ctx                = NULL;
    uniform_buffer_address = GPU_INVALID_ADDRESS;
    screen_color_handle    = GPU_INVALID_HANDLE;
    screen_depth_handle    = GPU_INVALID_HANDLE;
    screen_width           = 0;
    screen_height          = 0;
}

b32 graphics_render_frame(Mat4x4 camera_vp, Mat4x4 camera_iv) {
    static u32      opaque_lit_list_count = 0;
    static RenderId opaque_lit_list[1024] = {0};

    /* begin frame */ {
        u32 new_screen_width  = 0;
        u32 new_screen_height = 0;

        GpuResult frame_begin_result = gpu_cmd_screen_begin(gpu_ctx, &new_screen_width, &new_screen_height);
        if(frame_begin_result == GPU_RESULT_FAIL) {
            LOG_ERROR("failed to begin frame");
            goto fail;
        }
        if(frame_begin_result == GPU_RESULT_CLOSE) {
            goto close;
        }

        /* handle resources resize */
        if(new_screen_width != screen_width || new_screen_height != screen_height) {
            screen_width  = new_screen_width;
            screen_height = new_screen_height;

            if(!resize_screen_textures()) {
                LOG_ERROR("failed to resize screen textures");
                goto fail;
            }
        }
    }

    Mat4x4 camera_ivp = mat4x4_inverse(camera_vp);
    Frustrum frustrum = (Frustrum) {
        .left   = mat4x4_transform_plane(camera_ivp, (Vec4){-1.0, 0.0, 0.0, 1.0}),
        .right  = mat4x4_transform_plane(camera_ivp, (Vec4){ 1.0, 0.0, 0.0, 1.0}),
        .top    = mat4x4_transform_plane(camera_ivp, (Vec4){ 0.0,-1.0, 0.0, 1.0}),
        .bottom = mat4x4_transform_plane(camera_ivp, (Vec4){ 0.0, 1.0, 0.0, 1.0}),
        .near   = mat4x4_transform_plane(camera_ivp, (Vec4){ 0.0, 0.0, 1.0, 0.0}),
        .far    = mat4x4_transform_plane(camera_ivp, (Vec4){ 0.0, 0.0,-1.0, 1.0})
    };

    /* writes */
    UniformBuffer uniform_buffer = {
        .camera_vp     = camera_vp,
        .camera_iv     = camera_iv,
        .screen_params = (Vec4){(f32)screen_width, (f32)screen_height, 1.0f / (f32)screen_width, 1.0f / (f32)screen_height},
        .sun_dir       = {
            0.0, 
            1.0, 
            0.0, 
            0.0
        }
    };
    gpu_cmd_sync_memwrite(gpu_ctx, &uniform_buffer, sizeof(UniformBuffer), uniform_buffer_address);
    opaque_lit_list_count = opaque_lit_render_list(&frustrum, opaque_lit_list, ARRAY_SIZE(opaque_lit_list));

    /* demo pass */
    struct {f32 r; f32 g; f32 b; f32 a;} demo_push = {0.0, 1.0, 1.0, 1.0};
    gpu_cmd_targets_barrier(gpu_ctx, &screen_color_handle, 1, GPU_INVALID_HANDLE, TRUE, FALSE);
    gpu_cmd_begin_rendering(gpu_ctx, screen_width, screen_height);
    gpu_cmd_push_constants(gpu_ctx, &demo_push, sizeof(demo_push));
    gpu_cmd_bind_pipeline(gpu_ctx, PIPELINE_DEMO_ID);
    gpu_cmd_draw(gpu_ctx, 6, 1);
    gpu_cmd_end_rendering(gpu_ctx);

    /* axis pass */
    struct {u64 uniform_address;} axis_push = {uniform_buffer_address};
    gpu_cmd_targets_barrier(gpu_ctx, &screen_color_handle, 1, screen_depth_handle, TRUE, TRUE);
    gpu_cmd_begin_rendering(gpu_ctx, screen_width, screen_height);
    gpu_cmd_push_constants(gpu_ctx, &axis_push, sizeof(axis_push));
    gpu_cmd_bind_pipeline(gpu_ctx, PIPELINE_AXIS_ID);
    gpu_cmd_draw(gpu_ctx, 6 * 3, 1);
    gpu_cmd_end_rendering(gpu_ctx);

    /* planet pass */
    struct {u64 uniform_address; u32 resolution; u32 none_0;} planet_push = {uniform_buffer_address, 128, 0};
    gpu_cmd_targets_barrier(gpu_ctx, &screen_color_handle, 1, screen_depth_handle, FALSE, FALSE);
    gpu_cmd_begin_rendering(gpu_ctx, screen_width, screen_height);
    gpu_cmd_push_constants(gpu_ctx, &planet_push, sizeof(planet_push));
    gpu_cmd_bind_pipeline(gpu_ctx, PIPELINE_PLANET_ID);
    gpu_cmd_draw(gpu_ctx, 127 * 127 * 6, 1);
    gpu_cmd_end_rendering(gpu_ctx);

    /* default materials pass */
    gpu_cmd_targets_barrier(gpu_ctx, &screen_color_handle, 1, screen_depth_handle, FALSE, FALSE);
    gpu_cmd_begin_rendering(gpu_ctx, screen_width, screen_height);
    gpu_cmd_bind_pipeline(gpu_ctx, PIPELINE_DEFAULT_ID);
    for(u32 i = 0; i != opaque_lit_list_count; i++) {
        const GpuMesh*       mesh        = opaque_lit_list[i].gpu_mesh;
        const GpuImageHandle image       = opaque_lit_list[i].gpu_image; 
        const u32            material_id = opaque_lit_list[i].material_id;

        struct {
            u64 uniform_address; 
            u64 transform_address;
            u64 mesh_address;
            u32 image_id;
            u32 vertices_count;
            u32 indices_count;
        } default_push = {
            uniform_buffer_address,
            opaque_lit_gpu_adderss + (u64)material_id * sizeof(OpaqueLitMaterial),
            mesh->gpu_address,
            image,
            mesh->vertices_count,
            mesh->indices_count
        };

        gpu_cmd_push_constants(gpu_ctx, &default_push, sizeof(default_push));
        gpu_cmd_draw(gpu_ctx, mesh->indices_count, 1);
    }
    gpu_cmd_end_rendering(gpu_ctx);

    /* blit pass */
    struct {u32 src_image_id; u32 ms_count;} blit_push = {screen_color_handle, 4};
    gpu_cmd_sampled_barrier(gpu_ctx, screen_color_handle);
    gpu_cmd_sampled_barrier(gpu_ctx, screen_depth_handle);
    gpu_cmd_targets_barrier(gpu_ctx, (GpuImageHandle[]){GPU_SURFACE_IMAGE_ID}, 1, GPU_INVALID_HANDLE, TRUE, FALSE);
    gpu_cmd_begin_rendering(gpu_ctx, screen_width, screen_height);
    gpu_cmd_push_constants(gpu_ctx, &blit_push, sizeof(blit_push));
    gpu_cmd_bind_pipeline(gpu_ctx, PIPELINE_BLIT_ID);
    gpu_cmd_draw(gpu_ctx, 6, 1);
    gpu_cmd_end_rendering(gpu_ctx);

    /* end frame */ {
        GpuResult frame_end_result = gpu_cmd_screen_end(gpu_ctx);
        if(frame_end_result == GPU_RESULT_FAIL) {
            LOG_ERROR("failed to end frame");
            goto fail;
        }
        if(frame_end_result == GPU_RESULT_CLOSE) {
            goto close;
        }
    }

    return TRUE;

    fail: {
        return FALSE;
    }
    close: {
        return TRUE;
    }
}
