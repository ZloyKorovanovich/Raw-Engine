#include "resources.h"
#include "resources_structs.h"

#define CGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <stb/cgltf.h>
#include <stb/stb_image.h>

static GpuContext* gpu_ctx = NULL;

void resources_hook_gpu(GpuContext* context) {
    gpu_ctx = context;
}

b32 compare_keys(const ResourceKey* a, const ResourceKey* b) {
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

void* load_gltf_mesh(const char* path, Vertex** vertices_array, u32** indices_array, u32* vertices_count, u32* indices_count, f32* bounding_radius) {
    cgltf_options model_options = (cgltf_options){0};
    cgltf_data*   model_data    = NULL;

    /* load gltf model */ {
        if(cgltf_parse_file(&model_options, path, &model_data) != cgltf_result_success) {
            LOG_ERROR("failed to parse gltf file: \"%s\"", path);
            goto fail;
        }

        if(cgltf_load_buffers(&model_options, model_data, path) != cgltf_result_success) {
            LOG_ERROR("failed to load gltf buffers: \"%s\"", path);
            goto fail;
        }
    }

    void*   mesh_allocation     = NULL;
    Vertex* mesh_vertices       = NULL;
    u32*    mesh_indices        = NULL;
    u32     mesh_vertices_count = 0;
    u32     mesh_indices_count  = 0; 

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
        LOG_ERROR("invalid gltf mesh: \"%s\"", path);
        goto fail;
    }

    /* allocate mesh */
    const u64 alloc_size = mesh_vertices_count * sizeof(Vertex) + mesh_indices_count * sizeof(u32);
    mesh_allocation = malloc(alloc_size);
    if(mesh_allocation == NULL) {
        LOG_ERROR("failed to allocate mesh: \"%s\"", path);
        goto fail;
    }
    memset(mesh_allocation, 0, alloc_size);

    mesh_vertices = (void*)((u8*)mesh_allocation);
    mesh_indices  = (void*)((u8*)mesh_allocation + mesh_vertices_count * sizeof(Vertex));

    /* bounding sphere radius */
    f32 max_vertex_dist_sqr = 0.0;

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
            const cgltf_accessor* prim_uvs       = NULL;
            u32 prim_positions_count = 0;
            u32 prim_normals_count   = 0;
            u32 prim_uvs_count       = 0;

            for(u64 l = 0; l != attributes_count; l++) {
                if(attributes[l].type == cgltf_attribute_type_position && attributes[l].index == 0) {
                    prim_positions       = attributes[l].data;
                    prim_positions_count = (u32)attributes[l].data->count;
                }
                if(attributes[l].type == cgltf_attribute_type_normal && attributes[l].index == 0) {
                    prim_normals       = attributes[l].data;
                    prim_normals_count = (u32)attributes[l].data->count;
                }
                if(attributes[l].type == cgltf_attribute_type_texcoord && attributes[l].index == 0) {
                    prim_uvs           = attributes[l].data;
                    prim_uvs_count     = (u32)attributes[l].data->count;
                }
            }

            for(u32 i = 0; i != prim_positions_count; i++) {
                f32 position[3] = {0};
                f32 normal  [3] = {0};
                f32 uv      [2] = {0};

                if(i < prim_positions_count) {
                    cgltf_accessor_read_float(prim_positions, i, position, 3);
                }
                if(i < prim_normals_count) {
                    cgltf_accessor_read_float(prim_normals, i, normal, 3);
                }
                if(i < prim_uvs_count) {
                    cgltf_accessor_read_float(prim_uvs, i, uv, 2);
                }

                max_vertex_dist_sqr = MAX(max_vertex_dist_sqr, position[0] * position[0] + position[1] * position[1] + position[2] * position[2]);
                mesh_vertices[i] = (Vertex) {
                    .position = {position[0], position[1], position[2], 1.0},
                    .normal   = {normal  [0], normal  [1], normal  [2], 0.0},
                    .uv       = {uv      [0], uv      [1], 0.0        , 0.0}
                };
            }
        }
    }

    *vertices_array  = mesh_vertices;
    *indices_array   = mesh_indices;
    *vertices_count  = mesh_vertices_count;
    *indices_count   = mesh_indices_count;
    *bounding_radius = sqrtf(max_vertex_dist_sqr);
    cgltf_free(model_data);
    return mesh_allocation;

    fail: {
        free(mesh_allocation);
        if(model_data != NULL) {
            cgltf_free(model_data);
        }
        return NULL;
    }
}

b32 load_gltf_bound_radius(const char* path, f32* bounding_radius) {
    cgltf_options model_options = (cgltf_options){0};
    cgltf_data*   model_data    = NULL;

    /* load gltf model */ {
        if(cgltf_parse_file(&model_options, path, &model_data) != cgltf_result_success) {
            LOG_ERROR("failed to parse gltf file: \"%s\"", path);
            goto fail;
        }

        if(cgltf_load_buffers(&model_options, model_data, path) != cgltf_result_success) {
            LOG_ERROR("failed to load gltf buffers: \"%s\"", path);
            goto fail;
        }
    }

    f32 max_distance_sqr = 0;

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
                            const cgltf_accessor* prim_positions       = attributes[l].data;
                            const u32             prim_positions_count = (u32)attributes[l].data->count;
                            for(u32 v = 0; v != prim_positions_count; v++) {
                                f32 position[3] = {0};
                                cgltf_accessor_read_float(prim_positions, v, position, 3);
                                max_distance_sqr = MAX(max_distance_sqr, vec3_dot((Vec3){position[0], position[1], position[2]}, (Vec3){position[0], position[1], position[2]}));
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    *bounding_radius = sqrtf(max_distance_sqr);
    cgltf_free(model_data);
    return TRUE;

    fail: {
        *bounding_radius = 0.0;
        if(model_data != NULL) {
            cgltf_free(model_data);
        }
        return FALSE;
    }
}

static u32          meshes_count    = 0;
static u32          meshes_capacity = 0;
static Mesh*        meshes          = NULL;
static GpuMesh*     meshes_gpu      = NULL;
static ResourceKey* meshes_keys       = NULL;

MeshHandle mesh_register(const char* name) {
    ResourceKey key = (ResourceKey){0};
    strcpy_s(key.name, PATH_LENGTH, name);

    /* search for existsing mesh with same key */
    for(u32 i = 0; i != meshes_count; i++) {
        if(compare_keys(&key, &meshes_keys[i])) {
            return (MeshHandle)i;
        }
    }

    /* expand array  */
    if(meshes_count + 1 > meshes_capacity) {
        u32 const new_capacity = meshes_capacity + 1024;
        Mesh*        const new_meshes     = realloc(meshes     , new_capacity * sizeof(Mesh));
        GpuMesh*     const new_meshes_gpu = realloc(meshes_gpu , new_capacity * sizeof(GpuMesh));
        ResourceKey* const new_keys       = realloc(meshes_keys, new_capacity * sizeof(ResourceKey));

        meshes      = new_meshes     == NULL ? meshes       : new_meshes;
        meshes_gpu  = new_meshes_gpu == NULL ? meshes_gpu   : new_meshes_gpu;
        meshes_keys = new_keys       == NULL ? meshes_keys  : new_keys;

        /* failed to reallocate */
        if( new_meshes     == NULL || 
            new_keys       == NULL || 
            new_meshes_gpu == NULL
        ) {
            goto fail;
        }

        for(u32 i = 0; i != new_capacity - meshes_capacity; i++) {
            meshes[i]      = (Mesh){0};
            meshes_gpu[i]  = (GpuMesh){.gpu_address = GPU_INVALID_ADDRESS};
            meshes_keys[i] = (ResourceKey){0};
        }
        meshes_capacity = new_capacity;
    }

    /* load basic mesh info */
    f32 bounding_radius = 0.0;
    if(!load_gltf_bound_radius(name, &bounding_radius)) {
        LOG_ERROR("failed to open mesh");
        goto fail;
    }

    /* use lastest mesh slot */
    MeshHandle handle = meshes_count;
    meshes   [handle] = (Mesh){.bounding_radius = bounding_radius};
    meshes_keys[handle] = key;
    meshes_count++;

    return handle;

    fail: {
        return INVALID_RESOURCE;
    }
}

void mesh_free_all(void) {
    for(u32 i = 0; i != meshes_count; i++) {
        if(meshes_gpu[i].gpu_address != GPU_INVALID_ADDRESS) {
            gpu_free(gpu_ctx, meshes_gpu[i].gpu_address, meshes_gpu[i].gpu_size);
            meshes_gpu[i] = (GpuMesh){0};
        }
        if(meshes[i].allocation != NULL) {
            free(meshes[i].allocation);
            meshes[i] = (Mesh){0};
        }
    }
    free(meshes);
    free(meshes_keys);
}


const Mesh* mesh_load(MeshHandle mesh_handle) {
    if(mesh_handle >= meshes_count) {
        LOG_ERROR("invalid mesh handle");
        goto fail;
    }

    const char* mesh_name = meshes_keys[mesh_handle].name;

    if(meshes[mesh_handle].host_references == 0) {
        Vertex* vertices        = NULL;
        u32*    indices         = NULL;
        u32     vertices_count  = 0;
        u32     indices_count   = 0;
        f32     bounding_radius = 0.0;
        void* allocation = load_gltf_mesh(mesh_name, &vertices, &indices, &vertices_count, &indices_count, &bounding_radius);
        if(allocation == NULL) {
            LOG_ERROR("failed to load gltf mesh handle: %u name \"%s\"", mesh_handle, mesh_name);
            goto fail;
        }

        meshes[mesh_handle] = (Mesh) {
            .allocation      = allocation,
            .vertices        = vertices,
            .indices         = indices,
            .vertices_count  = vertices_count,
            .indices_count   = indices_count,
            .bounding_radius = bounding_radius,
            .host_references = 1
        };
    } else {
        meshes[mesh_handle].host_references++;
    }

    return &meshes[mesh_handle];

    fail: {
        return NULL;
    }
}

void mesh_unload(MeshHandle mesh_handle) {
    if(mesh_handle >= meshes_count) {
        LOG_ERROR("invalid mesh handle");
        return;
    }
    /* mesh already removed */
    if(meshes[mesh_handle].host_references == 0) {
        return;
    }

    /* remove mesh reference */
    if(meshes[mesh_handle].host_references - 1 == 0) {
        free(meshes[mesh_handle].allocation);
        f32 bounding_radius = meshes[mesh_handle].bounding_radius;
        meshes[mesh_handle] = (Mesh){.bounding_radius = bounding_radius};
    } else {
        meshes[mesh_handle].host_references--;
    }
}

/* FIX: currently only works while already recording command buffer */
const GpuMesh* mesh_load_gpu(MeshHandle mesh_handle) {
    if(mesh_handle >= meshes_count) {
        LOG_ERROR("invalid mesh handle");
        goto fail;
    }

    const Mesh* cpu_mesh    = NULL;
    u64         gpu_address = GPU_INVALID_ADDRESS;
    u64         gpu_size    = 0;

    if(meshes_gpu[mesh_handle].gpu_references == 0) {
        /* load mesh data on cpu */
        cpu_mesh = mesh_load(mesh_handle);
        if(cpu_mesh == NULL) {
            LOG_ERROR("failed to load cpu mesh");
            goto fail;
        }

        /* allocate gpu mesh buffer */
        gpu_size    = cpu_mesh->vertices_count * sizeof(Vertex) + cpu_mesh->indices_count * sizeof(u32);
        gpu_address = gpu_malloc(gpu_ctx, gpu_size, 16);
        if(gpu_address == GPU_INVALID_ADDRESS) {
            LOG_ERROR("failed to allocate gpu mesh buffer");
            goto fail;
        }

        /* copy cpu mesh to gpu */
        gpu_cmd_sync_memwrite(gpu_ctx, cpu_mesh->allocation, gpu_size, gpu_address);

        /* write mesh struct */
        meshes_gpu[mesh_handle] = (GpuMesh) {
            .gpu_address    = gpu_address,
            .gpu_size       = gpu_size,
            .vertices_count = cpu_mesh->vertices_count,
            .indices_count  = cpu_mesh->indices_count,
            .gpu_references = 1
        };
        
        /* unload mesh data from cpu */
        mesh_unload(mesh_handle);

    } else {
        meshes_gpu[mesh_handle].gpu_references++;
    }

    return &meshes_gpu[mesh_handle];

    fail: {
        if(gpu_address != GPU_INVALID_ADDRESS) {
            gpu_free(gpu_ctx, gpu_address, gpu_size);
        }
        if(cpu_mesh != NULL) {
            mesh_unload(mesh_handle);
        }
        return NULL;
    }
}

void mesh_unload_gpu(MeshHandle mesh_handle) {
    if(mesh_handle >= meshes_count) {
        LOG_ERROR("invalid mesh handle");
        return; 
    }
    /* mesh already removed */
    if(meshes_gpu[mesh_handle].gpu_references == 0) {
        return;
    }

    /* remove mesh reference */
    if(meshes_gpu[mesh_handle].gpu_references - 1 == 0) {
        gpu_free(gpu_ctx, meshes_gpu[mesh_handle].gpu_address, meshes_gpu[mesh_handle].gpu_size);
        meshes_gpu[mesh_handle] = (GpuMesh){0};
    } else {
        meshes_gpu[mesh_handle].gpu_references--;
    }
}

f32 mesh_get_bounding_radius(MeshHandle mesh_handle) {
    if(mesh_handle >= meshes_count) {
        LOG_ERROR("invalid mesh id");
        return 0.0;
    } else {
        return meshes[mesh_handle].bounding_radius;  
    }
}


static u32          textures_count    = 0;
static u32          textures_capacity = 0;
static Texture*     textures         = NULL;
static GpuTexture*  textures_gpu     = NULL;
static ResourceKey* textures_keys    = NULL;

TextureHandle texture_register(const char* name) {
    ResourceKey key = (ResourceKey){0};
    strcpy_s(key.name, PATH_LENGTH, name);
    
    for(u32 i = 0; i != textures_count; i++) {
        if(compare_keys(&key, &textures_keys[i])) {
            return (TextureHandle)i;
        }
    }

    if(textures_count + 1 > textures_capacity) {
        const u32 new_capacity = textures_capacity + 1024;
        Texture*     const new_textures      = realloc(textures     , new_capacity * sizeof(Texture));
        GpuTexture*  const new_textures_gpu  = realloc(textures_gpu , new_capacity * sizeof(GpuTexture));
        ResourceKey* const new_textures_keys = realloc(textures_keys, new_capacity * sizeof(ResourceKey));
    
        textures      = new_textures      == NULL ? textures      : new_textures;
        textures_gpu  = new_textures_gpu  == NULL ? textures_gpu  : new_textures_gpu;
        textures_keys = new_textures_keys == NULL ? textures_keys : new_textures_keys;

        if(new_textures == NULL || new_textures_gpu == NULL || new_textures_keys == NULL) {
            goto fail;
        }

        for(u32 i = 0; i != new_capacity - textures_capacity; i++) {
            textures     [i] = (Texture){0};
            textures_gpu [i] = (GpuTexture){.image_handle = GPU_INVALID_HANDLE};
            textures_keys[i] = (ResourceKey){0};
        }
        textures_capacity = new_capacity;
    }

    TextureHandle texture_id = textures_count;
    textures_keys[texture_id] = key;
    textures     [texture_id] = (Texture){0};
    textures_gpu [texture_id] = (GpuTexture){.image_handle = GPU_INVALID_HANDLE};
    textures_count++;

    return texture_id;

    fail: {
        return INVALID_RESOURCE;
    }
}

void texture_free_all(void) {
    for(u32 i = 0; i != textures_count; i++) {
        if(textures_gpu[i].image_handle != GPU_INVALID_HANDLE) {
            gpu_remove_image(gpu_ctx, textures_gpu[i].image_handle);
            textures_gpu[i] = (GpuTexture){.image_handle = GPU_INVALID_HANDLE};
        }
        if(textures[i].pixels_buffer != NULL) {
            free(textures[i].pixels_buffer);
            textures[i] = (Texture){0};
        }
        textures_keys[i] = (ResourceKey){0};
    }

    free(textures_gpu);
    free(textures);
    free(textures_keys);
    textures_count    = 0;
    textures_capacity = 0;
    textures          = NULL;
    textures_gpu      = NULL;
    textures_keys     = NULL;
}

const Texture* texture_load(TextureHandle texture_handle) {
    if(texture_handle >= textures_count) {
        LOG_ERROR("invalid texture handle");
        goto fail;
    }

    const char* name = textures_keys[texture_handle].name;
    stbi_uc* pixel_buffer = NULL;

    /* not loaded yet */
    if(textures[texture_handle].host_references == 0) {
        /* load texture data with stbi */
        i32 width    = 0;
        i32 height   = 0;
        i32 channels = 0;
        pixel_buffer = stbi_load(name, &width, &height, &channels, STBI_rgb_alpha);
        if(pixel_buffer == NULL) {
            LOG_ERROR("failed to load image handle: %u, name: \"%s\"", texture_handle, name);
            goto fail;
        }
        if(channels != 4) {
            LOG_ERROR("invalid channel count");
            goto fail;
        }

        textures[texture_handle] = (Texture) {
            .width           = (u32)width,
            .height          = (u32)height,
            .pixels_buffer   = (void*)pixel_buffer,
            .host_references = 1
        };
    } 
    /* already loaded */
    else {
        textures[texture_handle].host_references++;
    }

    return &textures[texture_handle];

    fail: {
        if(pixel_buffer != NULL) {
            stbi_image_free(pixel_buffer);
        }
        return NULL;
    }
}

void texture_unload(TextureHandle texture_handle) {
    if(texture_handle >= textures_count) {
        LOG_ERROR("invalid texture handle");
        return;
    }

    if(textures[texture_handle].host_references == 0) {
        return;
    }

    if(textures[texture_handle].host_references - 1 == 0) {
        stbi_image_free(textures[texture_handle].pixels_buffer);
        textures[texture_handle] = (Texture){0};
    } else {
        textures[texture_handle].host_references--;
    }
}

/* FIX: currently only works while already recording command buffer */
GpuImageHandle texture_load_gpu(TextureHandle texture_handle) {
    if(texture_handle >= textures_count) {
        LOG_ERROR("invalid texture handle");
        goto fail;
    }

    if(textures_gpu[texture_handle].gpu_references == 0) {
        /* load texture into RAM */
        const Texture* texture = texture_load(texture_handle);
        if(texture == NULL) {
            LOG_ERROR("failed to load texture");
            goto fail;
        }

        u32 texture_width     = texture->width;
        u32 texture_height    = texture->height;
        u32 texture_mip_count = (u32)(log2f((f32)MAX(texture_width, texture_height))) + 1;

        /* create gpu texture in pool */
        const GpuImageInfo image_info = (GpuImageInfo) {
            .flags     = GPU_IMAGE_FLAG_SAMPLED | GPU_IMAGE_FLAG_STORAGE,
            .format    = GPU_FORMAT_R8G8B8A8_UNORM,
            .width     = texture_width,
            .height    = texture_height,
            .mip_count = texture_mip_count,
            .ms_count  = 1 
        };
        GpuImageHandle image_handle = gpu_add_image(gpu_ctx, &image_info);
        if(image_handle == GPU_INVALID_HANDLE) {
            LOG_ERROR("failed to create gpu image");
            goto fail;
        }

        /* copy to gpu */
        gpu_cmd_sync_imagewrite(gpu_ctx, texture->pixels_buffer, (u64)texture->width * (u64)texture->height * 4, image_handle);
        gpu_cmd_generate_mip_maps(gpu_ctx, image_handle);
        gpu_cmd_sampled_barrier(gpu_ctx, image_handle);

        textures_gpu[texture_handle] = (GpuTexture) {
            .gpu_references = 1,
            .image_handle   = image_handle
        };
    } else {
        textures_gpu[texture_handle].gpu_references++;
    }

    return textures_gpu[texture_handle].image_handle;

    fail: {
        return GPU_INVALID_HANDLE;
    }
}

void texture_unload_gpu(TextureHandle texture_handle) {
    if(texture_handle >= textures_count) {
        LOG_ERROR("invalid texture handle");
        return;
    }

    if(textures_gpu[texture_handle].gpu_references == 0) {
        return;
    }

    if(textures_gpu[texture_handle].gpu_references - 1 == 0) {
        gpu_remove_image(gpu_ctx, textures_gpu[texture_handle].image_handle);
        textures_gpu[texture_handle] = (GpuTexture) {.image_handle = GPU_INVALID_HANDLE};
    } else {
        textures_gpu[texture_handle].gpu_references--;
    }
}
