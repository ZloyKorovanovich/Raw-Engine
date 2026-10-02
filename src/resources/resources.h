#ifndef _RESOURCES_INCLUDED
#define _RESOURCES_INCLUDED

#include "../base.h"
#include "../math/math.h"
#include "../gpu/gpu.h"

#define INVALID_RESOURCE (U32_MAX)

typedef u32 MeshHandle;
typedef u32 TextureHandle;

typedef struct {
    Vec4 position;
    Vec4 normal;
    Vec4 uv;
} Vertex;

/* allocation::[vertices:indices] */
typedef struct {
    void*   allocation;
    Vertex* vertices;
    u32*    indices;
    u32     vertices_count;
    u32     indices_count;
    u32     host_references;
    f32     bounding_radius;
} Mesh;

/* gpu_address::[vertices:indices] */
typedef struct {
    u64 gpu_address;
    u64 gpu_size;
    u32 vertices_count;
    u32 indices_count;
    u32 gpu_references;
} GpuMesh;

typedef struct {
    u32* pixels_buffer;
    u32  width;
    u32  height;
    u32  host_references;
} Texture;

typedef struct {
    GpuImageHandle image_handle;
    u32            gpu_references;
} GpuTexture;

void resources_hook_gpu(GpuContext* context);

MeshHandle     mesh_register(const char* name);
void           mesh_free_all(void);
const Mesh*    mesh_load(MeshHandle mesh_handle);
void           mesh_unload(MeshHandle mesh_handle);
const GpuMesh* mesh_load_gpu(MeshHandle mesh_handle);
void           mesh_unload_gpu(MeshHandle mesh_handle);
f32            mesh_get_bounding_radius(MeshHandle mesh_handle);

TextureHandle  texture_register(const char* name);
void           texture_free_all(void);
const Texture* texture_load(TextureHandle texture_handle);
void           texture_unload(TextureHandle texture_handle);
GpuImageHandle texture_load_gpu(TextureHandle texture_handle);
void           texture_unload_gpu(TextureHandle texture_handle);

#endif
