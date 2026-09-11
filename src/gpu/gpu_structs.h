#ifndef _GPU_STRUCTS_INCLUDED
#define _GPU_STRUCTS_INCLUDED

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "gpu.h"

#define GPU_PAGE_SIZE (4 * KB)
#define GPU_MAX_SWAPCHAIN_IMAGES (32)
#define GPU_OPTIMAL_SWAPCHAIN_COUNT (4)
#define GPU_MAX_PUSH_CONSTANTS (64)

#define GPU_BIDNING_SAMPLERS       (0)
#define GPU_BINDING_SAMPLED_IMAGES (1)
#define GPU_BINDING_STORAGE_IMAGES (2)

/* === resources === */

typedef struct {
    GpuPipelineFlags pipeline_flags;
    VkPipeline       pipeline;
} GpuPipeline;

typedef struct {
    GpuImageFlags        flags;
    VkFormat             format;
    VkImageAspectFlags   aspect;
    u32                  width;
    u32                  height;
    u32                  mip_count;
    VkImage              image;
    VkImageView          view;
    u64                  memory_offset;
    u64                  memory_size;
    VkImageLayout        layout;
    VkAccessFlags        access;
    VkPipelineStageFlags pipeline_stage;
} GpuImage;

/* === context === */

typedef struct {
    GLFWwindow*              window;
    VkInstance               instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkSurfaceKHR             surface;
} GpuVulkanObjects;

typedef struct {
    VkPhysicalDeviceType       device_type;
    u32                        pci_vendor_device;
    VkDevice                   device;
    VkPhysicalDevice           physical_device;
    VkQueue                    queue_render;
    VkQueue                    queue_transfer;
    u32                        queue_render_id;
    u32                        queue_transfer_id;
    VkColorSpaceKHR            surface_color_space;
    VkFormat                   surface_format;
    VkPresentModeKHR           present_mode;
    /* command buffers */
    VkCommandPool              command_pool_render;
    VkCommandPool              command_pool_transfer;
    VkCommandBuffer            command_buffer_render;
    VkCommandBuffer            command_buffer_transfer;
    /* extensions procs */
    PFN_vkCmdBeginRenderingKHR cmd_begin_rendering_khr;
    PFN_vkCmdEndRenderingKHR   cmd_end_rendering_khr;
} GpuVulkanDevice;

typedef struct {
    VkDeviceMemory        memory_malloc;
    VkDeviceMemory        memory_images;
    VkDeviceMemory        memory_transfer;
    void*                 map_malloc;
    void*                 map_images;
    void*                 map_transfer;
    VkMemoryPropertyFlags flags_images;
    VkMemoryPropertyFlags flags_malloc;
    VkMemoryPropertyFlags flags_transfer;
    u64                   size_malloc;
    u64                   size_images;
    u64                   size_transfer;
    /* buffers */
    u64                   offset_transfer_malloc;
    u64                   offset_transfer_images;
    VkBuffer              buffer_malloc;
    VkBuffer              buffer_transfer_malloc;
    VkBuffer              buffer_transfer_images;
    u64                   malloc_buffer_address;
    /* allocator */
    u64*                  pages_images_bits;
    u64*                  pages_malloc_bits;
    u64                   pages_images_count;
    u64                   pages_malloc_count;
} GpuMemoryPools;

typedef struct {
    /* surface */
    VkSwapchainKHR swapchain;
    VkImage        swapchain_images      [GPU_MAX_SWAPCHAIN_IMAGES];
    VkImageView    swapchain_images_views[GPU_MAX_SWAPCHAIN_IMAGES];
    u32            swapchain_images_count;
    u32            swapchain_width;
    u32            swapchain_height;
    /* samplers */
    VkSampler      sampler_linear_repeat;
    VkSampler      sampler_linear_clamp;
    VkSampler      sampler_nearest_repeat;
    VkSampler      sampler_nearest_clamp;
    /* descriptor set */
    VkDescriptorPool      descriptor_pool;
    VkDescriptorSet       descriptor_set;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout      pipeline_layout;
} GpuPersistentResources;

typedef struct {
    /* render queue */
    VkFence     render_fence;
    VkSemaphore image_acquire_semaphore;
    VkSemaphore image_submit_semaphores[GPU_MAX_SWAPCHAIN_IMAGES];
    /* transfer queue */
    VkFence     transfer_fence;
} GpuSyncObjects;

typedef struct {
    u32                       render_image_id;
    VkImage                   render_image;
    VkImageView               render_image_view;
    /* rendering attachments */
    u32                       color_attachments_count;
    b32                       use_depth_attachment;
    VkRenderingAttachmentInfo color_attachments[GPU_MAX_ATTACHMENTS];
    VkRenderingAttachmentInfo depth_attachment;
} GpuCmd;

typedef struct {
    GpuPipeline* pipelines;
    u32          pipelines_count;
} GpuPipelines;

typedef struct {
    void*           allocation;
    GpuImage*       images;
    GpuImageHandle* images_occupation;
    u32             images_max_count;
    u32             images_count;
} GpuImagesPool;


struct GpuContext {
    GpuVulkanObjects       vulkan_objects;
    GpuVulkanDevice        vulkan_device;
    GpuMemoryPools         memory_pools;
    GpuPersistentResources persistent_resources;
    GpuSyncObjects         sync_objects;
    GpuPipelines           pipelines;
    GpuImagesPool          images_pool;
    GpuCmd                 cmd;      
};

VkFormat gpu_convert_format(VkFormat surface_format, GpuFormat format, VkImageAspectFlags* image_aspect);
VkImageUsageFlags gpu_convert_image_usage(GpuImageFlags image_flags);
GpuResult gpu_recreate_swapchain(GpuContext* context);

#endif
