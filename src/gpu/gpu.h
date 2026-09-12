#ifndef _GPU_INCLUDED
#define _GPU_INCLUDED

/* this api intentionally is made as simple as possible. It's made to create indie games,
   not to ship AAA titles. It's just something that makes sense:
    - resources are cut down to 2 types memory buffers (device address) and images
    - images are stored in a pool
    - "buffers" use custom allocator
    - meshes should be treated as device address allocation as well
    - push constants are used to address resources
    - barriers are simplified to perform roughly like OpenGL or better for user simplicity 
    - fatal errors are fatal and no cleanup after them is expected 
    - shaders are expected to be compiled in spir-v format
    - all allocations (gpu_malloc, images) automatically free on termination, so you only need to have hot control over dynamic allocations
*/

#include "../base.h"

#define GPU_PCI_ANY (U32_MAX)
#define GPU_WINDOW_NAME_LENGTH (256)
#define GPU_MAX_ATTACHMENTS (8)
#define GPU_VERTEX_SHADER_ENTRY "vs_main"
#define GPU_FRAGMENT_SHADER_ENTRY "fs_main"
#define GPU_COMPUTE_SHADER_ENTRY "cs_main"
#define GPU_INVALID_ADDRESS (U64_MAX)

#define GPU_INVALID_HANDLE (0xFFFFFFFF)
#define GPU_SURFACE_IMAGE_ID (0xFFFFFFFE)

#define GPU_SAMPLER_LINEAR_REPEAT_ID  (0)
#define GPU_SAMPLER_LINEAR_CLAMP_ID   (1)
#define GPU_SAMPLER_NEAREST_REPEAT_ID (2)
#define GPU_SAMPLER_NEAREST_CLAMP_ID  (3)

typedef enum {
    GPU_RESULT_SUCCESS = 0,
    GPU_RESULT_CLOSE   = 1,
    GPU_RESULT_FAIL    = 2
} GpuResult;

typedef enum {
    GPU_PIPELINE_FLAG_COMPUTE         = 0x1
} GpuPipelineFlags;

typedef enum {
    GPU_CONFIG_DEBUG      = 0x1,
    GPU_CONFIG_FULLSCREEN = 0x2,
    GPU_CONFIG_VSYNC      = 0x4
} GpuConfigFlags;

typedef enum {
    GPU_MEMORY_MAP_HOST            = 0x01,
    GPU_MEMORY_FLAG_CPU_HEAP       = 0x02,
    GPU_MEMORY_FLAG_IMAGES         = 0x04,
    GPU_MEMORY_FLAG_DEVICE_ADDRESS = 0x08
} GpuMemoryFlags;

typedef enum {
    GPU_IMAGE_FLAG_COLOR_ATTACHMENT = 0x1,
    GPU_IMAGE_FLAG_DEPTH_ATTACHMENT = 0x2,
    GPU_IMAGE_FLAG_SAMPLED          = 0x4,
    GPU_IMAGE_FLAG_STORAGE          = 0x8
} GpuImageFlags;

typedef enum {
    GPU_FORMAT_NONE,
    GPU_FORMAT_R32G32B32A32_SFLOAT,
    GPU_FORMAT_R16G16B16A16_SFLOAT,
    GPU_FORMAT_D32_SFLOAT,
    GPU_FORMAT_SURFACE,
    GPU_FORMAT_COUNT
} GpuFormat;

typedef struct {
    char           window_name[GPU_WINDOW_NAME_LENGTH];
    GpuConfigFlags config_flags;
    u32            window_width;
    u32            window_height;
    u32            pci_vendor_device;
    u64            malloc_heap_size;
    u64            images_heap_size;
    u32            images_max_count;
} GpuInitInfo;

typedef struct {
    GpuPipelineFlags flags;
    union {
        struct {
            const char*       vertex_shader;
            const char*       fragment_shader;
            const GpuFormat*  color_formats;
            GpuFormat         depth_format;
            u32               color_formats_count;
            u32               ms_count;
        };
        struct {
            const char* compute_shader;
        };
    };
} GpuPipelineInfo;

typedef struct {
    GpuImageFlags flags;
    GpuFormat     format;
    u32           width;
    u32           height;
    u32           mip_count;
    u32           ms_count;
} GpuImageInfo;

typedef struct GpuContext GpuContext;
typedef u32 GpuPipelineHandle;
typedef u32 GpuImageHandle;

GpuContext* gpu_init(const GpuInitInfo* init_info);
void        gpu_terminate(GpuContext* context);

b32 gpu_compile_pipelines(GpuContext* context, const GpuPipelineInfo* pipeline_infos, u32 pipelines_count);

u64  gpu_malloc(GpuContext* context, u64 size, u64 alignment);
void gpu_free(GpuContext* context, u64 address, u64 size);
GpuImageHandle gpu_add_image(GpuContext* context, const GpuImageInfo* image_info);
void           gpu_remove_image(GpuContext* context, GpuImageHandle image_id);

GpuResult gpu_cmd_screen_begin(GpuContext* context, u32* screen_x, u32* screen_y);
GpuResult gpu_cmd_screen_end(GpuContext* context);
b32 gpu_cmd_offscreen_begin(GpuContext* context);
b32 gpu_cmd_offscreen_end(GpuContext* context);

void gpu_cmd_memory_barrier(GpuContext* context, u64 address, u64 size);
void gpu_cmd_targets_barrier(GpuContext* context, const GpuImageHandle* color_targets, u32 color_targets_count, GpuImageHandle depth_target, b32 clear_color, b32 clear_depth);
void gpu_cmd_sampled_barrier(GpuContext* context, const GpuImageHandle sampled_image);
void gpu_cmd_storage_barrier(GpuContext* context, const GpuImageHandle storage_image);

void gpu_cmd_begin_rendering(GpuContext* context, u32 width, u32 height);
void gpu_cmd_end_rendering(GpuContext* context);

void gpu_cmd_sync_memwrite(GpuContext* context, const void* data, u64 size, u64 address);
void gpu_cmd_bind_pipeline(GpuContext* context, GpuPipelineHandle pipeline_id);
void gpu_cmd_push_constants(GpuContext* context, const void* data, u32 size);
void gpu_cmd_draw(GpuContext* context, u32 vertices, u32 instances);
void gpu_cmd_dispatch(GpuContext* context, u32 groups_x, u32 groups_y, u32 groups_z);

/* used to hook inputs */
void* gpu_get_glfw_window(GpuContext* context);

#endif /* _GPU_INCLUDED */
