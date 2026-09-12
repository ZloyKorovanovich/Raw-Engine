#include "game.h"
#include "game_structs.h"
#include "../gpu/gpu.h"
#include "../math/math.h"
#include "../input/input.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>

/* rendering management code, pipelines, meshes, materials, culling, custom passes, all here
   - can assign multiple materials on the same entity 
   - custom post process/additional rendering in render frame function */

typedef struct {
    Mat4x4 camera_vp;
    Mat4x4 camera_iv;
    Vec4   sun_dir;
    Vec4   sun_color;
} UniformBuffer;

typedef struct {
    Mat4x4 matrix_m;
} DefaultMaterial;

typedef struct {
    Vec4 position;
    Vec4 normal;
} SimpleVertex;

/* === basic graphics === */

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

/* === screen textures === */

static GpuImageHandle screen_color_handle = GPU_INVALID_HANDLE;
static GpuImageHandle screen_depth_handle = GPU_INVALID_HANDLE;

static u32 screen_width  = 0;
static u32 screen_height = 0;

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

/* === meshes === */
/* dynamically allocated using gpu_malloc and gpu_free
   no removing from registery */
/* FIX: what if I add physics collision/ray casting? how my meshes will be traversed if they are not loaded ? 
   FIX: load only 1 node with 1 mesh with 1 primitive, ignore others */

typedef struct {
    u64 gpu_address;
    u64 gpu_size;
    u32 vertices_count;
    u32 indices_count;
} SimpleMesh;

typedef union {
    char name[PATH_LENGTH];
    u64  key [PATH_LENGTH / sizeof(u64)];
} MeshKey;

static u32         simple_meshes_capacity = 0;
static u32         simple_meshes_count    = 0;
static MeshKey*    simple_meshes_keys     = NULL;
static SimpleMesh* simple_meshes          = NULL;

b32 compare_mesh_keys(const MeshKey* a, const MeshKey* b) {
    u64* a_u64 = (u64*)a->key;
    u64* b_u64 = (u64*)b->key;
    b32  equal = TRUE;

    for(u32 i = 0; i != PATH_LENGTH / sizeof(u64); i++) {
        if(a_u64[i] != b_u64[i]) {
            equal = FALSE;
            break;
        }
    }

    return equal;
}

u32 register_mesh(const char* path) {
    /* check file existance */ {
        if(path == NULL) {
            LOG_ERROR("mesh path in null");
            goto fail;
        }

        FILE* file = fopen(path, "rb");
        if(file == NULL) {
            LOG_ERROR("mesh file doesnt exist: \"%s\"", path);
            goto fail;
        }
        fclose(file);
    }

    MeshKey search_key = (MeshKey){0};
    strcpy_s(search_key.name, PATH_LENGTH, path);

    /* find existing mesh in registry */
    u32 mesh_id = GPU_INVALID_HANDLE;
    for(u32 i = 0; i != simple_meshes_count; i++) {
        if(compare_mesh_keys(&search_key, &simple_meshes_keys[i])) {
            mesh_id = i;
            goto success;
        }
    }

    /* reallocate array if needed */
    if(simple_meshes_count + 1 > simple_meshes_capacity) {
        u32         new_capacity = simple_meshes_capacity + 512;
        SimpleMesh* new_meshes   = realloc(simple_meshes, new_capacity * sizeof(SimpleMesh));
        MeshKey*    new_keys     = realloc(simple_meshes_keys, new_capacity * sizeof(MeshKey));
        if(new_meshes == NULL || new_keys == NULL) {
            LOG_ERROR("failed to allocate new meshes");
            free(new_meshes);
            free(new_keys);
            goto fail;
        }

        simple_meshes_capacity = new_capacity;
        simple_meshes          = new_meshes;
        simple_meshes_keys     = new_keys;
    }

    /* append new mesh to array */
    simple_meshes     [simple_meshes_count] = (SimpleMesh){.gpu_address = GPU_INVALID_ADDRESS};
    simple_meshes_keys[simple_meshes_count] = search_key;
    mesh_id = simple_meshes_count;
    simple_meshes_count++;

    success: {
        return mesh_id;
    }

    fail: {
        return GPU_INVALID_HANDLE;
    }
}

void destroy_meshes(void) {
    free(simple_meshes);
    free(simple_meshes_keys);
    simple_meshes_count    = 0;
    simple_meshes_capacity = 0;
}

b32 upload_mesh(u32 mesh_id) {
    if(mesh_id >= simple_meshes_count) {
        LOG_ERROR("invalid mesh id");
        goto fail;
    }

    const char* name = simple_meshes_keys[mesh_id].name;

    cgltf_options model_options = (cgltf_options){0};
    cgltf_data*   model_data    = NULL;

    /* load gltf model */ {
        if(cgltf_parse_file(&model_options, name, &model_data) != cgltf_result_success) {
            LOG_ERROR("failed to parse gltf file: \"%s\"", name);
            goto fail;
        }

        if(cgltf_load_buffers(&model_options, model_data, name) != cgltf_result_success) {
            LOG_ERROR("failed to load gltf buffers: \"%s\"", name);
            goto fail;
        }
    }

    void*         mesh_allocation     = NULL;
    SimpleVertex* mesh_vertices       = NULL;
    u32*          mesh_indices        = NULL;
    u32           mesh_vertices_count = 0;
    u32           mesh_indices_count  = 0; 

     const cgltf_primitive* scanning_primitive = NULL;

    /* parse model */ {
        const u64         gltf_nodes_count = model_data->nodes_count;
        const cgltf_node* gltf_nodes       = model_data->nodes;

        for(u64 i = 0; i != gltf_nodes_count; i++) {
            if(gltf_nodes[i].mesh != NULL) {
                const u64              primitives_count = gltf_nodes[i].mesh->primitives_count;
                const cgltf_primitive* primitives       = gltf_nodes[i].mesh->primitives;
                
                for(u64 k = 0; k != primitives_count; k++) {
                    const u64              attributes_count = primitives[k].attributes_count;
                    const cgltf_attribute* attributes       = primitives[k].attributes;

                    for(u64 l = 0; l != attributes_count; l++) {
                        if(attributes[l].type == cgltf_attribute_type_position && attributes[l].data->count != 0 && attributes[l].index == 0) {
                            scanning_primitive  =& primitives[k];
                            mesh_indices_count  = (u32)primitives[k].indices->count;
                            mesh_vertices_count = (u32)attributes[l].data->count;
                            goto parsing_finished;
                        }
                    }
                }
            }
        }

        parsing_finished: {}
    }

    if(scanning_primitive == NULL || mesh_indices_count == 0 || mesh_vertices_count == 0) {
        LOG_ERROR("invalid gltf mesh: \"%s\"", name);
        goto fail;
    }

    /* allocate mesh */
    const u64 alloc_size = mesh_vertices_count * sizeof(SimpleVertex) + mesh_indices_count * sizeof(u32);
    mesh_allocation = malloc(alloc_size);
    if(mesh_allocation == NULL) {
        LOG_ERROR("failed to allocate mesh: \"%s\"", name);
        goto fail;
    }
    memset(mesh_allocation, 0, alloc_size);

    mesh_vertices = (void*)((u8*)mesh_allocation);
    mesh_indices  = (void*)((u8*)mesh_allocation + mesh_vertices_count * sizeof(SimpleVertex));

    /* copy meshes */ {
        const u64              attributes_count = scanning_primitive->attributes_count;
        const cgltf_attribute* attributes       = scanning_primitive->attributes;
        
        /* read indices */ {
            const cgltf_accessor* indices       = scanning_primitive->indices;
            const u32             indices_count = (u32)indices->count;
            for(u32 i = 0; i != indices_count; i++) {
                mesh_indices[i] = (u32)cgltf_accessor_read_index(indices, i);
            }
        }
            
        /* read vertices */ {
            const cgltf_accessor* prim_positions = NULL;
            const cgltf_accessor* prim_normals   = NULL;
            u32 prim_positions_count = 0;
            u32 prim_normals_count   = 0;

            for(u64 l = 0; l != attributes_count; l++) {
                if(attributes[l].type == cgltf_attribute_type_position && attributes[l].index == 0) {
                    prim_positions       = attributes[l].data;
                    prim_positions_count = (u32)attributes[l].data->count;
                }
                if(attributes[l].type == cgltf_attribute_type_normal && attributes[l].index == 0) {
                    prim_normals       = attributes[l].data;
                    prim_normals_count = (u32)attributes[l].data->count;
                }
            }

            for(u32 i = 0; i != prim_positions_count; i++) {
                f32 position[3] = {0};
                f32 normal  [3] = {0};

                if(i < prim_positions_count) {
                    cgltf_accessor_read_float(prim_positions, i, position, 3);
                }
                if(i < prim_normals_count) {
                    cgltf_accessor_read_float(prim_normals, i, normal, 3);
                }

                mesh_vertices[i] = (SimpleVertex) {
                    .position = {position[0], position[1], position[2], 1.0},
                    .normal   = {normal  [0], normal  [1], normal  [2], 0.0}
                };
            }
        }
    }

    u64 gpu_address = gpu_malloc(gpu_ctx, alloc_size, 16);
    if(gpu_address == GPU_INVALID_ADDRESS) {
        LOG_ERROR("failed to allocate gpu memory");
        goto fail;
    }

    gpu_cmd_sync_memwrite(gpu_ctx, mesh_allocation, alloc_size, gpu_address);

    /* set mesh to upload mode */
    simple_meshes[mesh_id] = (SimpleMesh) {
        .gpu_address   = gpu_address,
        .gpu_size      = alloc_size,
        .indices_count = mesh_indices_count,
        .vertices_count = mesh_vertices_count
    };

    free(mesh_allocation);
    cgltf_free(model_data);
    return TRUE;

    fail: {
        free(mesh_allocation);
        if(model_data != NULL) {
            cgltf_free(model_data);
        }
        return FALSE;
    }
}

/* === default material === */
/* entities [<this is rendered (if not less)> : default_materials_buffer_count : <this is hidden> : default_entities_count : <this is free> : default_entities_capacity] */

typedef struct {
    Entity* entity;
    u64     entity_id;
    u32     mesh_id;
} EntityReference;

static u32  default_materials_buffer_count   = 0;
static u64  default_materials_buffer_address = GPU_INVALID_ADDRESS;

static u32  default_culled_entities_count = 0;
static u32* default_culled_entities       = NULL; 

static DefaultMaterial* default_materials         = NULL;
static EntityReference* default_entities          = NULL;
static u32              default_entities_count    = 0;
static u32              default_entities_capacity = 0;

void remove_default_materials_by_id(u32 id) {
    if(id == default_entities_count - 1) {
        default_entities [id] = (EntityReference){0};
        default_materials[id] = (DefaultMaterial){0};
        default_entities_count--;
    } else {
        default_entities [id] = default_entities [default_entities_count - 1];
        default_materials[id] = (DefaultMaterial){0};
        default_entities [id].entity->is_updated = TRUE;

        default_entities [default_entities_count - 1] = (EntityReference){0};
        default_materials[default_entities_count - 1] = (DefaultMaterial){0};
        default_entities_count--;
    }
}

b32 graphics_default_materials_add(Entity* entity) {
    /* allocate gpu buffer if needed */ {
        if(default_materials_buffer_address == GPU_INVALID_ADDRESS) {
            default_materials_buffer_count   = 512;
            default_materials_buffer_address = gpu_malloc(gpu_ctx, default_materials_buffer_count * sizeof(DefaultMaterial), 16);
            if(default_materials_buffer_address == GPU_INVALID_ADDRESS) {
                LOG_ERROR("failed to allocate default materials buffer");
                goto fail;
            }

            default_culled_entities_count   = 0;
            default_culled_entities = calloc(default_materials_buffer_count, sizeof(u32));
            if(default_culled_entities == NULL) {
                LOG_ERROR("failed to allocate cull buffer");
                goto fail;
            }
        }
    }

    /* reallocate cpu */ {
        if(default_entities_count + 1 > default_entities_capacity) {
            u32 new_capacity = default_entities_capacity + 512;
            DefaultMaterial* new_materials = realloc(default_materials, new_capacity * sizeof(DefaultMaterial));
            EntityReference* new_entities  = realloc(default_entities , new_capacity * sizeof(EntityReference));
            if(new_materials == NULL || new_entities == NULL) {
                LOG_ERROR("failed to reallocate entities arrays");
                free(new_materials);
                free(new_entities);
                goto fail;
            }

            default_materials         = new_materials;
            default_entities          = new_entities;
            default_entities_capacity = new_capacity;
        }
    }

    /* register mesh */
    u32 mesh_id = register_mesh(entity->mesh_name);
    if(mesh_id == GPU_INVALID_HANDLE) {
        LOG_ERROR("failed to register mesh");
        goto fail;
    }

    /* add new entity to array */
    default_entities[default_entities_count] = (EntityReference) {
        .entity    = entity,
        .entity_id = entity->entity_id,
        .mesh_id   = mesh_id
    };
    default_materials[default_entities_count] = (DefaultMaterial) {0};
    default_entities_count++;
    entity->is_updated = TRUE;

    return TRUE;

    fail: {
        return FALSE;
    }
}

void graphics_default_materials_remove(Entity* entity) {
    u64 entity_id = entity->entity_id;
    for(u32 i = 0; i != default_entities_count; i++) {
        if(default_entities[i].entity == entity && default_entities[i].entity_id == entity_id) {
            remove_default_materials_by_id(i);
        }
    }
}

void graphics_default_materials_clear(void) {
    free(default_culled_entities);
    default_culled_entities       = NULL;
    default_culled_entities_count = 0;

    free(default_materials);
    free(default_entities);
    default_materials               = NULL;
    default_entities                = NULL;
    default_entities_count          = 0;
    default_entities_capacity       = 0;
}

void upload_default_materials(void) {
    u64 upload_offset_begin = U64_MAX;
    u64 upload_offset_end   = 0;

    for(u32 i = 0; i != MIN(default_materials_buffer_count, default_entities_count); i++) {
        Entity* entity = default_entities[i].entity;
        
        /* if entity disappeared */
        if(entity == NULL || entity->entity_id != default_entities[i].entity_id) {
            remove_default_materials_by_id(i);
            i--;
            continue;
        }

        /* fix material if updated */
        if(entity->is_updated) {
            entity->is_updated = FALSE;
            default_materials[i] = (DefaultMaterial) {
                .matrix_m = mat4x4_mul_mat4x4(mat4x4_translation(entity->position), mat4x4_rotation(entity->rotation))
            };

            upload_offset_begin = MIN(i * sizeof(DefaultMaterial), upload_offset_begin);
            upload_offset_end   = MAX((i + 1) * sizeof(DefaultMaterial), upload_offset_end);
        }

        if(simple_meshes[default_entities[i].mesh_id].gpu_address == GPU_INVALID_ADDRESS) {
            if(!upload_mesh(default_entities[i].mesh_id)) {
                LOG_ERROR("failed to upload mesh");
                goto fail;
            }
        }
    }

    if(upload_offset_begin < upload_offset_end) {
        gpu_cmd_sync_memwrite(gpu_ctx, (u8*)default_materials + upload_offset_begin, upload_offset_end - upload_offset_begin, default_materials_buffer_address + upload_offset_begin);
    }

    fail: {}
}

/* === init/terminate === */

b32 graphics_render_frame(Mat4x4 camera_vp, Mat4x4 camera_iv) {
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

    /* writes */
    UniformBuffer uniform_buffer = {
        .camera_vp = camera_vp,
        .camera_iv = camera_iv,
        .sun_dir = {
            0.0, 
            1.0, 
            0.0, 
            0.0
        }
    };
    gpu_cmd_sync_memwrite(gpu_ctx, &uniform_buffer, sizeof(UniformBuffer), uniform_buffer_address);
    upload_default_materials();

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
    for(u32 i = 0; i != MIN(default_materials_buffer_count, default_entities_count); i++) {
        struct {
            u64 uniform_address; 
            u64 transform_address;
            u64 mesh_address;
            u32 vertices_count;
            u32 indices_count;
        } default_push = {
            uniform_buffer_address, 
            default_materials_buffer_address + i * sizeof(DefaultMaterial),
            simple_meshes[default_entities[i].mesh_id].gpu_address,
            simple_meshes[default_entities[i].mesh_id].vertices_count,
            simple_meshes[default_entities[i].mesh_id].indices_count
        };

        gpu_cmd_push_constants(gpu_ctx, &default_push, sizeof(default_push));
        gpu_cmd_draw(gpu_ctx, simple_meshes[default_entities[i].mesh_id].indices_count, 1);
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

b32 graphics_init(b32 is_debug) {
    const GpuInitInfo gpu_info = {
        .window_name        = "demo",
        .window_width       = 800,
        .window_height      = 600,
        .config_flags       = is_debug ? GPU_CONFIG_DEBUG | GPU_CONFIG_VSYNC : GPU_CONFIG_VSYNC,
        .pci_vendor_device  = GPU_PCI_ANY,
        .malloc_heap_size   = 256 * MB,
        .images_heap_size   = 256 * MB,
        .images_max_count   = 1024
    };

    gpu_ctx = gpu_init(&gpu_info);
    if(gpu_ctx == NULL) {
        LOG_ERROR("failed to init gpu");
        goto fail;
    }

    /* create pipelines */ {
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
                .vertex_shader       = "./out/data/blit_v.spv",
                .fragment_shader     = "./out/data/blit_f.spv",
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
                .flags               = 0,
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
    }

    /* create buffers */ {
        uniform_buffer_address = gpu_malloc(gpu_ctx, sizeof(UniformBuffer), 16);
        if(uniform_buffer_address == GPU_INVALID_ADDRESS) {
            LOG_ERROR("failed to allocate uniform buffer");
            goto fail;
        }
    }

    input_hook_window(gpu_get_glfw_window(gpu_ctx));

    return TRUE;

    fail: {
        return FALSE;
    }
}

void graphics_terminate(void) {
    graphics_default_materials_clear();
    destroy_meshes();
    gpu_terminate(gpu_ctx);
    gpu_ctx                = NULL;
    uniform_buffer_address = GPU_INVALID_ADDRESS;
}
