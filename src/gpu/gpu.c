#include "gpu.h"
#include "gpu_structs.h"

/* ==== ==== ==== ==== ==== ==== ==== ==== ==== 
    conversion tables
   ==== ==== ==== ==== ==== ==== ==== ==== ==== */

typedef struct {
    VkFormat           format;
    VkImageAspectFlags aspect;
} GpuFormatConversion;

const GpuFormatConversion gpu_format_conversion_table[GPU_FORMAT_COUNT] = {
    [GPU_FORMAT_NONE               ] = {VK_FORMAT_UNDEFINED          , VK_IMAGE_ASPECT_NONE     },
    [GPU_FORMAT_R32G32B32A32_SFLOAT] = {VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT},
    [GPU_FORMAT_R16G16B16A16_SFLOAT] = {VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT},
    [GPU_FORMAT_D32_SFLOAT         ] = {VK_FORMAT_D32_SFLOAT         , VK_IMAGE_ASPECT_DEPTH_BIT},
    [GPU_FORMAT_SURFACE            ] = {VK_FORMAT_UNDEFINED          , VK_IMAGE_ASPECT_NONE     }
};

VkFormat gpu_convert_format(VkFormat surface_format, GpuFormat format, VkImageAspectFlags* image_aspect) {
    /* no aspect */
    if(image_aspect == NULL) {
        if(format > GPU_FORMAT_COUNT) {
            return VK_FORMAT_UNDEFINED;
        } else if(format == GPU_FORMAT_SURFACE) {
            return surface_format;
        } else {
            return gpu_format_conversion_table[format].format;
        }
    }
    /* with aspect */
    else {
        if(format > GPU_FORMAT_COUNT) {
            *image_aspect = VK_IMAGE_ASPECT_NONE;   
            return VK_FORMAT_UNDEFINED;
        } else if(format == GPU_FORMAT_SURFACE) {
            *image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
            return surface_format;
        } else {
            *image_aspect = gpu_format_conversion_table[format].aspect; 
            return gpu_format_conversion_table[format].format;
        }
    }
    
}

VkImageUsageFlags gpu_convert_image_usage(GpuImageFlags image_flags) {
    VkImageUsageFlags usage = 0;

    if(image_flags & GPU_IMAGE_FLAG_COLOR_ATTACHMENT) {
        usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if(image_flags & GPU_IMAGE_FLAG_DEPTH_ATTACHMENT) {
        usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    if(image_flags & GPU_IMAGE_FLAG_SAMPLED) {
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if(image_flags & GPU_IMAGE_FLAG_STORAGE) {
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    
    return usage;
}

/* ==== ==== ==== ==== ==== ==== ==== ==== ==== 
    helpers                                    
   ==== ==== ==== ==== ==== ==== ==== ==== ==== */

VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity, 
    VkDebugUtilsMessageTypeFlagsEXT             type, 
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, 
    void*                                       user_data
) {
    printf("[vulkan]: %s\n", callback_data->pMessage);
    return (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? FALSE : TRUE;
}

/* ==== ==== ==== ==== ==== ==== ==== ==== ==== 
    initialization
   ==== ==== ==== ==== ==== ==== ==== ==== ==== */

/* === instance & glfw === */

b32 create_vulkan_objects(const char* name, u32 width, u32 height, GpuConfigFlags config_flags, GpuVulkanObjects* objects) {
    const b32 is_debug = config_flags & GPU_CONFIG_DEBUG;
    GLFWwindow*              window          = NULL;
    VkInstance               instance        = NULL;
    VkDebugUtilsMessengerEXT debug_messenger = NULL;
    VkSurfaceKHR             surface         = NULL;

    /* create glfw window */ {
        if(!glfwInit()) {
            LOG_ERROR("failed to init glfw");
            goto fail;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow((i32)width, (i32)height, name, NULL, NULL);
        if(window == NULL) {
            LOG_ERROR("failed to create window");
            goto fail;
        }
    }

    /* create vulkan instance */ {
        const char* debug_layers[]     = { "VK_LAYER_KHRONOS_validation" };
        const char* debug_extensions[] = { "VK_EXT_debug_utils" };

        /* glfw extensions */
        u32          surface_extensions_count = 0;
        const char** surface_extensions = glfwGetRequiredInstanceExtensions(&surface_extensions_count);

        /* full instance extensions array */
        u32          instance_extensions_count = is_debug ? surface_extensions_count + ARRAY_SIZE(debug_extensions) : surface_extensions_count;
        const char** instance_extensions       = calloc(instance_extensions_count, sizeof(const char*));
        if(instance_extensions == NULL) {
            LOG_ERROR("failed to allocate instance extensions array");
            goto fail;
        }
        for(u32 i = 0; i != instance_extensions_count; i++) {
            instance_extensions[i] = (i < surface_extensions_count) ? surface_extensions[i] : debug_extensions[i - surface_extensions_count];
        }

        /* create infos */
        const VkApplicationInfo app_info = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = name,
            .pEngineName      = name,
            .engineVersion    = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion       = VK_API_VERSION_1_3
        };
        const VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
            .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debug_messenger_callback
        };
        const VkInstanceCreateInfo instance_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo        = &app_info,
            .enabledExtensionCount   = instance_extensions_count,
            .ppEnabledExtensionNames = instance_extensions,
            .enabledLayerCount       = is_debug ? ARRAY_SIZE(debug_layers) : 0,
            .ppEnabledLayerNames     = is_debug ? debug_layers             : NULL,
            .pNext                   = is_debug ? &debug_messenger_info    : NULL
        };

        /* create instance */
        if(vkCreateInstance(&instance_info, NULL, &instance) != VK_SUCCESS) {
            LOG_ERROR("failed to create vulkan instance");
            goto fail;
        }

        /* create debug messenger */
        if(is_debug) {
            PFN_vkCreateDebugUtilsMessengerEXT create_debug_messenger_ext = (void*)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
            if(create_debug_messenger_ext == NULL) {
                LOG_ERROR("failed to create debug messenger");
                goto fail;
            }
            if(create_debug_messenger_ext(instance, &debug_messenger_info, NULL, &debug_messenger) != VK_SUCCESS) {
                LOG_ERROR("failed to create debug messenger");
                goto fail;
            }
        }

        free(instance_extensions);
    }

    /* create surface */ {
        if(glfwCreateWindowSurface(instance, window, NULL, &surface) != VK_SUCCESS) {
            LOG_ERROR("failed to create glfw surface");
            goto fail;
        }
    }

    *objects = (GpuVulkanObjects) {
        .window          = window,
        .instance        = instance,
        .debug_messenger = debug_messenger,
        .surface         = surface
    };
    return TRUE;

    fail: {
        return FALSE;
    }
}

void destroy_vulkan_objects(GpuVulkanObjects* vulkan_objects) {
    VkInstance               instance        = vulkan_objects->instance;
    VkSurfaceKHR             surface         = vulkan_objects->surface;
    VkDebugUtilsMessengerEXT debug_messenger = vulkan_objects->debug_messenger;
    GLFWwindow*              window          = vulkan_objects->window;

    vkDestroySurfaceKHR(instance, surface, NULL);
    glfwDestroyWindow(window);
    if(debug_messenger != NULL) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_messenger = (void*)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if(destroy_debug_messenger != NULL) {
            destroy_debug_messenger(instance, debug_messenger, NULL);
        }
    }
    vkDestroyInstance(instance, NULL);
}

/* === vulkan device === */
/* FIX: the lack of trasfer queue is not critical, if you fail to create it might be also not critical
   FIX: add priotities when selecting device */

b32 check_physical_device(GpuConfigFlags config_flags, VkPhysicalDevice physical_device, VkSurfaceKHR surface, u64 required_heap_size, GpuVulkanDevice* device) {
    b32 vsync_on = config_flags & GPU_CONFIG_VSYNC;

    VkPhysicalDeviceType device_type = VK_PHYSICAL_DEVICE_TYPE_OTHER;
    u32 pci_vendor = U32_MAX;
    u32 pci_device = U32_MAX;

    u32               present_modes_count = 0;
    VkPresentModeKHR* present_modes       = NULL;
    u32                    present_extensions_count = 0;
    VkExtensionProperties* present_extensions       = NULL;
    u32                      queue_families_count = 0;
    VkQueueFamilyProperties* queue_families       = NULL;
    u32                 surface_formats_count = 0;
    VkSurfaceFormatKHR* surface_formats       = NULL;

    /* check device properties */ {
        VkPhysicalDeviceProperties2 device_properties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        vkGetPhysicalDeviceProperties2(physical_device, &device_properties);

        device_type = device_properties.properties.deviceType;
        pci_vendor  = device_properties.properties.vendorID;
        pci_device  = device_properties.properties.deviceID;

        if(device_type != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && device_type != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            goto fail;
        }
    }

    /* FIX: should be more smart (check actual heap sizes for images etc) */
    /* check memory heap size */ {
        VkPhysicalDeviceMemoryProperties2 memory_properties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
        vkGetPhysicalDeviceMemoryProperties2(physical_device, &memory_properties);
        
        u32           memory_heaps_count = memory_properties.memoryProperties.memoryHeapCount; 
        VkMemoryHeap* memory_heaps       = memory_properties.memoryProperties.memoryHeaps;

        u64 available_host_heap_size   = 0;
        u64 available_device_heap_size = 0;
        for(u32 i = 0; i != memory_heaps_count; i++) {
            if(memory_heaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                available_device_heap_size += memory_heaps[i].size;
            } else {
                available_host_heap_size += memory_heaps[i].size;
            }
        }

        if(available_host_heap_size < required_heap_size && available_device_heap_size < required_heap_size) {
            goto fail;
        }
    }

    /* check features */ {
        VkPhysicalDeviceVulkan13Features features_1_3 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceVulkan12Features features_1_2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = &features_1_3};
        VkPhysicalDeviceFeatures2 device_features     = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &features_1_2};

        vkGetPhysicalDeviceFeatures2(physical_device, &device_features);
        if( !device_features.features.shaderStorageImageWriteWithoutFormat ||
            !device_features.features.shaderStorageImageReadWithoutFormat  ||
            !device_features.features.shaderInt64 ||
            !features_1_2.descriptorIndexing ||
            !features_1_2.descriptorBindingSampledImageUpdateAfterBind ||
            !features_1_2.descriptorBindingStorageImageUpdateAfterBind ||
            !features_1_2.bufferDeviceAddress ||
            !features_1_2.descriptorBindingPartiallyBound || 
            !features_1_3.dynamicRendering
        ) {
            goto fail;
        }
    }

    vkEnumerateDeviceExtensionProperties(physical_device, NULL, &present_extensions_count, NULL);
    present_extensions = calloc(present_extensions_count, sizeof(VkExtensionProperties));
    if(present_extensions == NULL) {
        goto fail;
    }
    vkEnumerateDeviceExtensionProperties(physical_device, NULL, &present_extensions_count, present_extensions);

    /* check extensions presence */ {
        const char* device_extensions[] = {
            "VK_KHR_swapchain",
            "VK_KHR_dynamic_rendering"
        };

        for(u32 i = 0; i != ARRAY_SIZE(device_extensions); i++) {
            b32 found_extension = FALSE;
            for(u32 k = 0; k != present_extensions_count; k++) {
                if(strcmp(device_extensions[i], present_extensions[k].extensionName) == 0) {
                    found_extension = TRUE;
                    break;
                }
            }
            if(!found_extension) {
                goto fail;
            }
        }
    }

    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, NULL);
    queue_families = calloc(queue_families_count, sizeof(VkQueueFamilyProperties));
    if(queue_families == NULL) {
        goto fail;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, queue_families);

    u32 queue_render_id   = U32_MAX;
    u32 queue_transfer_id = U32_MAX;

    /* find queues */ {
        const VkQueueFlags queue_flags_mask     = VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT;
        const VkQueueFlags queue_flags_render   = VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT;
        const VkQueueFlags queue_flags_transfer = VK_QUEUE_TRANSFER_BIT;

        for(u32 i = 0; i != queue_families_count; i++) {
            if( queue_render_id == U32_MAX        &&
                queue_families[i].queueCount != 0 &&
                (queue_families[i].queueFlags & queue_flags_mask) == queue_flags_render
            ) {
                queue_render_id = i;
                continue;
            }
            if( queue_transfer_id == U32_MAX      &&
                queue_families[i].queueCount != 0 &&
                (queue_families[i].queueFlags & queue_flags_mask) == queue_flags_transfer
            ) {
                queue_transfer_id = i;
                continue;
            }
        }

        if(queue_render_id == U32_MAX) {
            goto fail;
        }
    }

    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_formats_count, NULL);
    surface_formats = calloc(surface_formats_count, sizeof(VkSurfaceFormatKHR));
    if(surface_formats == NULL) {
        goto fail;
    }
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_formats_count, surface_formats);

    VkSurfaceFormatKHR surface_format = (VkSurfaceFormatKHR){.format = VK_FORMAT_UNDEFINED};

    /* find surface format */ {
        if(surface_formats_count == 0) {
            goto fail;
        }

        /* find some format */
        surface_format =  surface_formats[0];
        /* optimal format */
        for(u32 i = 0; i != surface_formats_count; i++) {
            if( surface_formats[i].format     == VK_FORMAT_R8G8B8A8_UNORM && 
                surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            ) {
                surface_format = surface_formats[i];
                break;
            }
        }

        /* failed to find any format */
        if(surface_format.format == VK_FORMAT_UNDEFINED) {
            goto fail;
        }
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_modes_count, NULL);
    present_modes = calloc(present_modes_count, sizeof(VkPresentModeKHR));
    if(present_modes == NULL) {
        goto fail;
    }
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_modes_count, present_modes);

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    
    /* find present mode */ {
        if(present_modes_count == 0) {
            goto fail;
        }

        for(u32 i = 0; i != present_modes_count; i++) {
            if(present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                present_mode = present_modes[i];
            }
            if(!vsync_on) {
                if(present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                    present_mode = present_modes[i];
                }
            }
        }
    }

    free(present_modes);
    free(surface_formats);
    free(queue_families);
    free(present_extensions);

    *device = (GpuVulkanDevice) {
        .device_type         = device_type,
        .pci_vendor_device   = ((pci_vendor << 16) & 0xFFFF0000) | (pci_device & 0x0000FFFF),
        .physical_device     = physical_device,
        .surface_format      = surface_format.format,
        .surface_color_space = surface_format.colorSpace,
        .present_mode        = present_mode,
        .queue_render_id     = queue_render_id,
        .queue_transfer_id   = queue_transfer_id
    };

    return TRUE;

    fail: {
        free(present_modes);
        free(surface_formats);
        free(queue_families);
        free(present_extensions);
        return FALSE;
    }
}

b32 create_vulkan_device(GpuConfigFlags config_flags, u32 pci_vendor_device, u64 total_heap_size, const GpuVulkanObjects* objects, GpuVulkanDevice* device) {
    VkInstance   instance = objects->instance;
    VkSurfaceKHR surface  = objects->surface;

    /* get devices */
    u32               physical_devices_count = 0;
    VkPhysicalDevice* physical_devices       = NULL;
    vkEnumeratePhysicalDevices(instance, &physical_devices_count, NULL);
    physical_devices = calloc(physical_devices_count, sizeof(VkPhysicalDevice));
    if(physical_devices == NULL) {
        LOG_ERROR("failed to allocate physical device array");
        goto fail;
    }
    vkEnumeratePhysicalDevices(instance, &physical_devices_count, physical_devices);

    GpuVulkanDevice selected_device = (GpuVulkanDevice){0};

    /* select device */ {
        b32 found_device = FALSE;
        for(u32 i = 0; i != physical_devices_count; i++) {
            if(check_physical_device(config_flags, physical_devices[i], surface, total_heap_size, &selected_device)) {
                if(pci_vendor_device == GPU_PCI_ANY) {
                    found_device = TRUE;
                    break;
                } else if (selected_device.pci_vendor_device == pci_vendor_device) {
                    found_device = TRUE;
                    break;
                }
            }
        }
        if(!found_device) {
            LOG_ERROR("failed to find suitable gpu adapter");
            goto fail;
        }
    }

    /* create device */ {
        const b32 has_transfer_queue = selected_device.queue_transfer_id != U32_MAX;
        const char* device_extensions[] = {
            "VK_KHR_swapchain",
            "VK_KHR_dynamic_rendering"
        };

        /* queue create infos */
        const f32 queue_priority           = 1.0;
        const u32 queue_create_infos_count = has_transfer_queue ? 2 : 1; 
        const VkDeviceQueueCreateInfo queue_create_infos[] = {
            (VkDeviceQueueCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pQueuePriorities = &queue_priority,
                .queueCount       = 1,
                .queueFamilyIndex = selected_device.queue_render_id
            },
            (VkDeviceQueueCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pQueuePriorities = &queue_priority,
                .queueCount       = 1,
                .queueFamilyIndex = selected_device.queue_transfer_id
            }
        };

        /* features */
        const VkPhysicalDeviceVulkan13Features features_1_3 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .dynamicRendering = TRUE
        };
        const VkPhysicalDeviceVulkan12Features features_1_2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .descriptorIndexing                           = TRUE,
            .bufferDeviceAddress                          = TRUE,
            .descriptorBindingPartiallyBound              = TRUE,
            .descriptorBindingSampledImageUpdateAfterBind = TRUE,
            .descriptorBindingStorageImageUpdateAfterBind = TRUE,
            .pNext = (void*)&features_1_3
        };
        const VkPhysicalDeviceFeatures2 device_features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .features = {
                .shaderStorageImageWriteWithoutFormat = TRUE,
                .shaderStorageImageReadWithoutFormat  = TRUE,
                .shaderInt64                          = TRUE
            },
            .pNext = (void*)&features_1_2
        };

        /* create device */
        const VkDeviceCreateInfo device_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .enabledExtensionCount   = ARRAY_SIZE(device_extensions),
            .ppEnabledExtensionNames = device_extensions,
            .queueCreateInfoCount    = queue_create_infos_count,
            .pQueueCreateInfos       = queue_create_infos,
            .pNext                   = (void*)&device_features
        };
        if(vkCreateDevice(selected_device.physical_device, &device_info, NULL, &selected_device.device) != VK_SUCCESS) {
            LOG_ERROR("failed to create vulkan device");
            goto fail;
        }

        /* get queues */
        vkGetDeviceQueue(selected_device.device, selected_device.queue_render_id, 0, &selected_device.queue_render);
        if(has_transfer_queue) {
            vkGetDeviceQueue(selected_device.device, selected_device.queue_transfer_id, 0, &selected_device.queue_transfer);
        }
    }

    /* create command buffers */ {
        /* command pools */
        const VkCommandPoolCreateInfo command_pool_render_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = selected_device.queue_render_id
        };
        const VkCommandPoolCreateInfo command_pool_transfer_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = selected_device.queue_transfer_id
        };

        if(vkCreateCommandPool(selected_device.device, &command_pool_render_info, NULL, &selected_device.command_pool_render) != VK_SUCCESS) {
            LOG_ERROR("failed to create render command pool");
            goto fail;
        }
        if(selected_device.queue_transfer != NULL) {
            if(vkCreateCommandPool(selected_device.device, &command_pool_transfer_info, NULL, &selected_device.command_pool_transfer) != VK_SUCCESS) {
                LOG_ERROR("failed to create transfer command pool");
                goto fail;
            }
        }

        /* command buffers */
        const VkCommandBufferAllocateInfo render_command_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandPool        = selected_device.command_pool_render,
            .commandBufferCount = 1
        };
        const VkCommandBufferAllocateInfo transfer_command_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandPool        = selected_device.command_pool_transfer,
            .commandBufferCount = 1
        };

        if(vkAllocateCommandBuffers(selected_device.device, &render_command_buffer_info, &selected_device.command_buffer_render) != VK_SUCCESS) {
            LOG_ERROR("failed to allocate render command buffer");
            goto fail;
        }
        if(selected_device.queue_transfer != NULL) {
            if(vkAllocateCommandBuffers(selected_device.device, &transfer_command_buffer_info, &selected_device.command_buffer_transfer) != VK_SUCCESS) {
                LOG_ERROR("failed to allocate transfer command buffer");
                goto fail;
            }
        }
    }

    /* load extensions */ {
        selected_device.cmd_begin_rendering_khr = (void*)vkGetDeviceProcAddr(selected_device.device, "vkCmdBeginRenderingKHR");
        selected_device.cmd_end_rendering_khr   = (void*)vkGetDeviceProcAddr(selected_device.device, "vkCmdEndRenderingKHR");

        if( selected_device.cmd_begin_rendering_khr == NULL ||
            selected_device.cmd_end_rendering_khr   == NULL
        ) {
            LOG_ERROR("failed to load device extenions procs");
            goto fail;
        }
    }
    
    *device = selected_device;
    free(physical_devices);
    return TRUE;

    fail: {
        free(physical_devices);
        return FALSE;
    }
}

void destroy_vulkan_device(const GpuVulkanObjects* vulkan_objects, GpuVulkanDevice* vulkan_device) {
        VkDevice      device                = vulkan_device->device;
        VkCommandPool command_pool_render   = vulkan_device->command_pool_render;
        VkCommandPool command_pool_transfer = vulkan_device->command_pool_transfer;
      
        vkDestroyCommandPool(device, command_pool_render, NULL);
        if(command_pool_transfer != NULL) {
            vkDestroyCommandPool(device, command_pool_transfer, NULL);
        }
        vkDestroyDevice(device, NULL);
}

/* === memory === */
/* FIX: heap size reduction doesnt actually work, because heaps overlap especially with ReBAR extension */

u64* create_memory_allocator(u64 heap_size, u64* allocator_pages_count) {
    if(heap_size > U64_MAX - (GPU_PAGE_SIZE - 1)) {
        goto fail;
    }

    /* on pages blocks count overflow is not possible, as GPU_PAGE_SIZE is much bigger than 64 */
    u64 pages_count        = (heap_size + GPU_PAGE_SIZE - 1) / GPU_PAGE_SIZE;
    u64 pages_blocks_count = (pages_count + 63) / 64;

    /* allocate and mark all as free */
    u64* pages_blocks = calloc(pages_blocks_count, sizeof(u64));
    if(pages_blocks == NULL) {
        goto fail;
    }

    /* mark unused as occupied */
    for(u64 i = pages_count; i != pages_blocks_count * 64; i++) {
        u64 block_id = i / 64;
        u64 page_id  = i % 64;
        pages_blocks[block_id] |= 1llu << page_id;
    }

    *allocator_pages_count = pages_count;
    return pages_blocks;

    fail: {
        return NULL;
    }
}

u64 allocate_memory_allocator(u64* pages_bits, u64 pages_count, u64 size, u64 alignment) {
    if( size == 0 ||
        alignment == 0 ||
        size + GPU_PAGE_SIZE - 1 < size
    ) {
        goto fail;
    }

    u64 required_pages_count = (size + GPU_PAGE_SIZE - 1) / GPU_PAGE_SIZE;
    u64 offset_begin   = U64_MAX;
    u64 pages_begin_id = U64_MAX;
    u64 pages_in_row   = 0;

    for(u64 i = 0; i != pages_count && pages_in_row != required_pages_count; i++) {
        u64 block_id = i / 64;
        u64 page_id  = i % 64;

        if(!(pages_bits[block_id] & (1llu << page_id))) {
            if(pages_in_row == 0) {
                if((i * GPU_PAGE_SIZE) % alignment == 0) {
                    offset_begin   = i * GPU_PAGE_SIZE;
                    pages_begin_id = i;
                    pages_in_row++;
                } else {
                    continue;
                }
            } else {
                pages_in_row++;
            }
        } else {
            offset_begin   = U64_MAX;
            pages_begin_id = U64_MAX;
            pages_in_row   = 0;
        }
    }

    if(pages_in_row != required_pages_count) {
        goto fail;
    }

    for(u64 i = pages_begin_id; i != pages_begin_id + pages_in_row; i++) {
        u64 block_id = i / 64;
        u64 page_id  = i % 64;
        pages_bits[block_id] |= 1llu << page_id;
    }

    return offset_begin;

    fail: {
        return U64_MAX;
    }
}

b32 free_memory_allocator(u64* pages_bits, u64 pages_count, u64 offset, u64 size) {
    if( size == 0 ||
        offset % GPU_PAGE_SIZE != 0 || 
        offset > U64_MAX - size ||
        offset + size > U64_MAX - (GPU_PAGE_SIZE - 1)
    ) {
        goto fail;
    }

    u64 pages_free_begin = offset / GPU_PAGE_SIZE;
    u64 pages_free_count = (size + GPU_PAGE_SIZE - 1) / GPU_PAGE_SIZE;

    if(pages_free_begin + pages_free_count > pages_count) {
        goto fail;
    }

    for(u64 i = pages_free_begin; i != pages_free_begin + pages_free_count; i++) {
        u64 block_id = i / 64;
        u64 page_id  = i % 64;
        if(!(pages_bits[block_id] & (1llu << page_id))) {
            goto fail;
        }
    }

    for(u64 i = pages_free_begin; i != pages_free_begin + pages_free_count; i++) {
        u64 block_id = i / 64;
        u64 page_id  = i % 64;
        pages_bits[block_id] &= ~(1llu << page_id);
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

const VkMemoryPropertyFlags memory_flags_device_only[] = {
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  ,

    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ,

    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT,

    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
};

const VkMemoryPropertyFlags memory_flags_device_shared[] = {
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  ,

    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  ,

    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
};

const VkMemoryPropertyFlags memory_flags_host[] = {
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  ,

    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  ,

    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  ,

    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
};

u32 find_memory_type(VkPhysicalDeviceMemoryProperties* memory_properties, VkPhysicalDeviceType device_type, u64 size, u32 type_bits, b32 device_local, VkMemoryPropertyFlags* memory_flags) {
    if(size == 0) {
        goto fail;
    }

    const VkMemoryPropertyFlags* memory_flags_list        = NULL;
    u32                          memory_flags_list_length = 0;

    /* select list of prioritized memory types */ 
    if(device_type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        memory_flags_list        = device_local ?            memory_flags_device_only    :            memory_flags_host;
        memory_flags_list_length = device_local ? ARRAY_SIZE(memory_flags_device_only)   : ARRAY_SIZE(memory_flags_host);
    } else {
        memory_flags_list        = device_local ?            memory_flags_device_shared  :            memory_flags_host;
        memory_flags_list_length = device_local ? ARRAY_SIZE(memory_flags_device_shared) : ARRAY_SIZE(memory_flags_host);
    }

    /* find memory type */
    const u32           memory_types_count = memory_properties->memoryTypeCount;
    const VkMemoryType* memory_types       = memory_properties->memoryTypes;
    VkMemoryHeap*       memory_heaps       = memory_properties->memoryHeaps;

    u32 selected_type_id = U32_MAX;
    u32 selected_heap_id = U32_MAX;

    for(u32 i = 0; i != memory_flags_list_length; i++) {
        for(u32 j = 0; j != memory_types_count; j++) {
            if(
                memory_heaps[memory_types[j].heapIndex].size >= size &&
                memory_types[j].propertyFlags == memory_flags_list[i]
            ) {
                /* perfect type */
                if(type_bits & (0x1 << j)) {
                    selected_type_id = j;
                    selected_heap_id = memory_types[j].heapIndex;
                    goto found_memory_type;
                }
                /* worse type (might be not perfect for certtain resource)*/
                if(selected_type_id == U32_MAX) {
                    selected_type_id = j;
                    selected_heap_id = memory_types[j].heapIndex;
                }
            }
        }
    }

    found_memory_type: {}

    if(selected_type_id != U32_MAX && selected_heap_id != U32_MAX) {
        memory_heaps[selected_heap_id].size = memory_heaps[selected_heap_id].size - size;
        *memory_flags = memory_types[selected_type_id].propertyFlags;
        return selected_type_id;
    }
    
    fail: {
        return U32_MAX;
    }
}

b32 create_memory_pools(u64 malloc_heap_size, u64 images_heap_size, const GpuVulkanDevice* vulkan_device, GpuMemoryPools* memory_pools) {
    VkPhysicalDeviceType device_type     = vulkan_device->device_type;
    VkPhysicalDevice     physical_device = vulkan_device->physical_device;
    VkDevice             device          = vulkan_device->device;
    
    /* suitable memory type bits */
    u32 bits_malloc   = 0;
    u32 bits_images   = 0;
    u32 bits_transfer = 0;

    u64 alignment_images   = 0;
    u64 alignment_malloc   = 0;
    u64 alignment_transfer = 0;

    /* get memory type bits */ {
        VkImage  dummy_color_image     = NULL;
        VkImage  dummy_depth_image     = NULL;
        VkBuffer dummy_malloc_buffer   = NULL;
        VkBuffer dummy_transfer_buffer = NULL;
    
        const VkImageCreateInfo image_color_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType   = VK_IMAGE_TYPE_2D,
            .usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .format      = VK_FORMAT_R32G32B32A32_SFLOAT,
            .samples     = VK_SAMPLE_COUNT_1_BIT,
            .extent      = {1024, 1024, 1},
            .mipLevels   = 1,
            .arrayLayers = 1
        };
        const VkImageCreateInfo image_depth_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType   = VK_IMAGE_TYPE_2D,
            .usage       = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .format      = VK_FORMAT_D32_SFLOAT,
            .samples     = VK_SAMPLE_COUNT_1_BIT,
            .extent      = {1024, 1024, 1},
            .mipLevels   = 1,
            .arrayLayers = 1
        };
        const VkBufferCreateInfo buffer_malloc_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .size  = 4096
        };
        const VkBufferCreateInfo buffer_transfer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .size  = 4096
        };

        if(vkCreateImage(device, &image_color_info, NULL, &dummy_color_image) != VK_SUCCESS) {
            LOG_ERROR("failed to create dummy color image");
            goto fail;
        }
        if(vkCreateImage(device, &image_depth_info, NULL, &dummy_depth_image) != VK_SUCCESS) {
            LOG_ERROR("failed to create dummy depth image");
            goto fail;
        }
        if(vkCreateBuffer(device, &buffer_malloc_info, NULL, &dummy_malloc_buffer) != VK_SUCCESS) {
            LOG_ERROR("failed to create dummy malloc buffer");
            goto fail;
        }
        if(vkCreateBuffer(device, &buffer_transfer_info, NULL, &dummy_transfer_buffer) != VK_SUCCESS) {
            LOG_ERROR("failed to create dummy transfer buffer");
            goto fail;
        }

        VkMemoryRequirements image_color_requirements     = (VkMemoryRequirements){0};
        VkMemoryRequirements image_depth_requirements     = (VkMemoryRequirements){0};
        VkMemoryRequirements buffer_malloc_requirements   = (VkMemoryRequirements){0};
        VkMemoryRequirements buffer_transfer_requirements = (VkMemoryRequirements){0};

        vkGetImageMemoryRequirements(device, dummy_color_image, &image_color_requirements);
        vkGetImageMemoryRequirements(device, dummy_depth_image, &image_depth_requirements);
        vkGetBufferMemoryRequirements(device, dummy_malloc_buffer, &buffer_malloc_requirements);
        vkGetBufferMemoryRequirements(device, dummy_transfer_buffer, &buffer_transfer_requirements);

        bits_malloc   = buffer_malloc_requirements.memoryTypeBits;
        bits_transfer = buffer_transfer_requirements.memoryTypeBits;
        bits_images   = image_color_requirements.memoryTypeBits & image_depth_requirements.memoryTypeBits;

        alignment_malloc   = buffer_malloc_requirements.alignment;
        alignment_transfer = buffer_transfer_requirements.alignment;
        alignment_images   = MAX(image_color_requirements.alignment, image_depth_requirements.alignment);

        vkDestroyImage(device, dummy_color_image, NULL);
        vkDestroyImage(device, dummy_depth_image, NULL);
        vkDestroyBuffer(device, dummy_malloc_buffer, NULL);
        vkDestroyBuffer(device, dummy_transfer_buffer, NULL);
    }

    images_heap_size = ALIGN(images_heap_size, alignment_images);
    malloc_heap_size = ALIGN(malloc_heap_size, alignment_malloc);
    u64 transfer_heap_size = 0;

    u32 type_id_images   = U32_MAX;
    u32 type_id_malloc   = U32_MAX;
    u32 type_id_transfer = U32_MAX;

    VkMemoryPropertyFlags flags_images   = 0;
    VkMemoryPropertyFlags flags_malloc   = 0;
    VkMemoryPropertyFlags flags_transfer = 0;

    u64 transfer_images_offset = 0;
    u64 transfer_malloc_offset = 0;
    b32 use_transfer_images    = FALSE;
    b32 use_transfer_malloc    = FALSE; 

    /* find memory types */ {
        VkPhysicalDeviceMemoryProperties2 memory_properties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
        vkGetPhysicalDeviceMemoryProperties2(physical_device, &memory_properties);

        if(images_heap_size != 0) {
            type_id_images = find_memory_type(&memory_properties.memoryProperties, device_type, images_heap_size, bits_images, TRUE, &flags_images);
            if(type_id_images == U32_MAX) {
                LOG_ERROR("failed to find images memory type");
                goto fail;
            }
        }
        if(malloc_heap_size != 0) {
            type_id_malloc = find_memory_type(&memory_properties.memoryProperties, device_type, malloc_heap_size, bits_malloc, TRUE, &flags_malloc);
            if(type_id_malloc == U32_MAX) {
                LOG_ERROR("failed to find malloc memory type");
                goto fail;
            }
        }

        /* calculate transfer block */
        const u64 alignment_transfer_images = MAX(alignment_images, alignment_transfer);
        const u64 alignment_transfer_malloc = MAX(alignment_malloc, alignment_transfer);

        if(!(flags_images & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            use_transfer_images    = TRUE;
            transfer_images_offset = ALIGN(transfer_heap_size, alignment_transfer_images);
            transfer_heap_size     = ALIGN(transfer_images_offset + images_heap_size, alignment_transfer_images);
        }
        if(!(flags_malloc & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            use_transfer_malloc    = TRUE;
            transfer_malloc_offset = ALIGN(transfer_heap_size, alignment_transfer_malloc);
            transfer_heap_size     = ALIGN(transfer_malloc_offset + malloc_heap_size, alignment_transfer_malloc);
        }

        if(transfer_heap_size != 0) {
            type_id_transfer = find_memory_type(&memory_properties.memoryProperties, device_type, transfer_heap_size, bits_transfer, FALSE, &flags_transfer);
            if(type_id_transfer == U32_MAX) {
                LOG_ERROR("failed to find transfer memory type");
            }
        }
    }

    VkDeviceMemory memory_malloc   = NULL;
    VkDeviceMemory memory_images   = NULL;
    VkDeviceMemory memory_transfer = NULL;
    void*          map_malloc      = NULL;
    void*          map_images      = NULL;
    void*          map_transfer    = NULL;

    /* allocate memory */ {
        /* alloc infos */
        const VkMemoryAllocateFlagsInfo malloc_allocate_flags = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
        };
        const VkMemoryAllocateInfo malloc_allocate_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .memoryTypeIndex = type_id_malloc,
            .allocationSize  = malloc_heap_size,
            .pNext           = (void*)&malloc_allocate_flags
        };
        const VkMemoryAllocateInfo images_allocate_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .memoryTypeIndex = type_id_images,
            .allocationSize  = images_heap_size
        };
        const VkMemoryAllocateInfo transfer_allocate_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .memoryTypeIndex = type_id_transfer,
            .allocationSize  = transfer_heap_size
        };

        /* allocate and map */
        if(malloc_heap_size != 0) {
            if(vkAllocateMemory(device, &malloc_allocate_info, NULL, &memory_malloc) != VK_SUCCESS) {
                LOG_ERROR("failed to allocate malloc memory");
                goto fail;
            }
            if(!use_transfer_malloc) {
                if(vkMapMemory(device, memory_malloc, 0, malloc_heap_size, 0, &map_malloc) != VK_SUCCESS) {
                    LOG_ERROR("failed to map malloc memory");
                    goto fail;
                }
            }
        }
        if(images_heap_size != 0) {
            if(vkAllocateMemory(device, &images_allocate_info, NULL, &memory_images) != VK_SUCCESS) {
                LOG_ERROR("failed to allocate images memory");
                goto fail;
            }
            if(!use_transfer_images) {
                if(vkMapMemory(device, memory_images, 0, images_heap_size, 0, &map_images) != VK_SUCCESS) {
                    LOG_ERROR("failed to map images memory");
                    goto fail;
                }
            }
        }
        if(transfer_heap_size != 0) {
            if(vkAllocateMemory(device, &transfer_allocate_info, NULL, &memory_transfer) != VK_SUCCESS) {
                LOG_ERROR("failed to allocate transfer memory");
                goto fail;
            }
            if(vkMapMemory(device, memory_transfer, 0, transfer_heap_size, 0, &map_transfer) != VK_SUCCESS) {
                LOG_ERROR("failed to map transfer memory");
                goto fail;
            }
        }
    }

    VkBuffer buffer_malloc          = NULL;
    VkBuffer buffer_transfer_images = NULL;
    VkBuffer buffer_transfer_malloc = NULL;
    u64      malloc_buffer_address  = 0;

    /* create buffers */ {
        const VkBufferCreateInfo buffer_malloc_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .size  = malloc_heap_size
        };
        const VkBufferCreateInfo buffer_transfer_images_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .size  = images_heap_size
        };
        const VkBufferCreateInfo buffer_transfer_malloc_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .size  = malloc_heap_size
        };

        /* create malloc buffer */
        if(malloc_heap_size != 0) {
            if(vkCreateBuffer(device, &buffer_malloc_info, NULL, &buffer_malloc) != VK_SUCCESS) {
                LOG_ERROR("failed to create malloc buffer");
                goto fail;
            }
            if(vkBindBufferMemory(device, buffer_malloc, memory_malloc, 0) != VK_SUCCESS) {
                LOG_ERROR("failed to bind malloc buffer memory");
                goto fail;
            }

            /* get device address of malloc buffer */
            const VkBufferDeviceAddressInfo buffer_device_address_info = {
                .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = buffer_malloc
            };
            malloc_buffer_address = vkGetBufferDeviceAddress(device, &buffer_device_address_info);
            if(malloc_buffer_address == 0) {
                LOG_ERROR("failed to get malloc buffer device address");
                goto fail;
            }
        }
        /* create transfer buffers */
        if(transfer_heap_size != 0) {
            if(use_transfer_images) {
                if(vkCreateBuffer(device, &buffer_transfer_images_info, NULL, &buffer_transfer_images) != VK_SUCCESS) {
                    LOG_ERROR("failed to create transfer images buffer");
                    goto fail;
                }
                if(vkBindBufferMemory(device, buffer_transfer_images, memory_transfer, transfer_images_offset) != VK_SUCCESS) {
                    LOG_ERROR("failed to bind transfer images buffer memory");
                    goto fail;
                }
            }
            if(use_transfer_malloc) {
                if(vkCreateBuffer(device, &buffer_transfer_malloc_info, NULL, &buffer_transfer_malloc) != VK_SUCCESS) {
                    LOG_ERROR("failed to create transfer malloc buffer");
                    goto fail;
                }
                if(vkBindBufferMemory(device, buffer_transfer_malloc, memory_transfer, transfer_malloc_offset) != VK_SUCCESS) {
                    LOG_ERROR("failed to bind transfer malloc buffer memory");
                    goto fail;
                }
            }
        }
    }

    u64* pages_images_bits  = NULL;
    u64* pages_malloc_bits  = NULL;
    u64  pages_images_count = 0;
    u64  pages_malloc_count = 0;

    /* create allocators */ {
        if(images_heap_size != 0) {
            pages_images_bits = create_memory_allocator(images_heap_size, &pages_images_count);
            if(pages_images_bits == NULL) {
                LOG_ERROR("failed to create gpu images allocator");
                goto fail;
            }
        }
        if(malloc_heap_size != 0) {
            pages_malloc_bits = create_memory_allocator(malloc_heap_size, &pages_malloc_count);
            if(pages_malloc_bits == NULL) {
                LOG_ERROR("failed to create gpu malloc allocator");
                goto fail;
            }
        }
    }

    *memory_pools = (GpuMemoryPools) {
        .memory_images          = memory_images,
        .memory_malloc          = memory_malloc,
        .memory_transfer        = memory_transfer,
        .map_images             = map_images,
        .map_malloc             = map_malloc,
        .map_transfer           = map_transfer,
        .size_images            = images_heap_size,
        .size_malloc            = malloc_heap_size,
        .size_transfer          = transfer_heap_size,
        .offset_transfer_images = transfer_images_offset,
        .offset_transfer_malloc = transfer_malloc_offset,
        .buffer_malloc          = buffer_malloc,
        .buffer_transfer_images = buffer_transfer_images,
        .buffer_transfer_malloc = buffer_transfer_malloc,
        .malloc_buffer_address  = malloc_buffer_address,
        .flags_transfer         = flags_transfer,
        .flags_malloc           = flags_malloc,
        .flags_images           = flags_images,
        .pages_images_bits      = pages_images_bits,
        .pages_malloc_bits      = pages_malloc_bits,
        .pages_images_count     = pages_images_count,
        .pages_malloc_count     = pages_malloc_count
    };

    return TRUE;

    fail: {
        return FALSE;
    }
}

void destroy_memory_pools(const GpuVulkanDevice* vulkan_device, GpuMemoryPools* memory_pools) {
    VkDevice device = vulkan_device->device;

    u64* pages_images_bits = memory_pools->pages_images_bits;
    u64* pages_malloc_bits = memory_pools->pages_malloc_bits;

    if(pages_images_bits != NULL) {
        free(pages_images_bits);
    }
    if(pages_malloc_bits != NULL) {
        free(pages_malloc_bits);
    }

    VkBuffer buffer_transfer_images = memory_pools->buffer_transfer_images;
    VkBuffer buffer_transfer_malloc = memory_pools->buffer_transfer_malloc;
    VkBuffer buffer_malloc          = memory_pools->buffer_malloc;

    if(buffer_transfer_images != NULL) {
        vkDestroyBuffer(device, buffer_transfer_images, NULL);
    }
    if(buffer_transfer_malloc != NULL) {
        vkDestroyBuffer(device, buffer_transfer_malloc, NULL);
    }
    if(buffer_malloc != NULL) {
        vkDestroyBuffer(device, buffer_malloc, NULL);
    }

    void* map_images   = memory_pools->map_images;
    void* map_malloc   = memory_pools->map_malloc;
    void* map_transfer = memory_pools->map_transfer; 
    VkDeviceMemory memory_images   = memory_pools->memory_images;
    VkDeviceMemory memory_malloc   = memory_pools->memory_malloc;
    VkDeviceMemory memory_transfer = memory_pools->memory_transfer;

    if(memory_images != NULL) {
        if(map_images != NULL) {
            vkUnmapMemory(device, memory_images);
        }
        vkFreeMemory(device, memory_images, NULL);
    }
    if(memory_malloc != NULL) {
        if(map_malloc != NULL) {
            vkUnmapMemory(device, memory_malloc);
        }
        vkFreeMemory(device, memory_malloc, NULL);
    }
    if(memory_transfer != NULL) {
        if(map_transfer != NULL) {
            vkUnmapMemory(device, memory_transfer);
        }
        vkFreeMemory(device, memory_transfer, NULL);
    }

    *memory_pools = (GpuMemoryPools){0};
}

/* === persistent resources === */
/* FIX: idealy should not cerate any image descriptors if max image count = 0, but for now is clamped to 1 */

GpuResult create_swapchain(
    const GpuVulkanObjects* vulkan_objects,
    const GpuVulkanDevice*  vulkan_device,
    VkSwapchainKHR*         swapchain, 
    VkImage*                images, 
    VkImageView*            views, 
    u32*                    images_count,
    u32*                    swapchain_x,
    u32*                    swapchain_y,
    b32                     can_close
) {
    GLFWwindow*      window  = vulkan_objects->window;
    VkSurfaceKHR     surface = vulkan_objects->surface;
    VkDevice         device              = vulkan_device->device;
    VkPhysicalDevice physical_device     = vulkan_device->physical_device;
    VkColorSpaceKHR  surface_color_space = vulkan_device->surface_color_space;
    VkFormat         surface_format      = vulkan_device->surface_format;
    VkPresentModeKHR present_mode        = vulkan_device->present_mode;

    VkSwapchainKHR old_swapchain    = *swapchain;
    u32            old_images_count = *images_count;

    VkSurfaceCapabilitiesKHR surface_capabilities = (VkSurfaceCapabilitiesKHR){0};

    /* wait untill window is shown */ {
        while(1) {
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);
            if(surface_capabilities.currentExtent.width != 0 && surface_capabilities.currentExtent.height != 0) {
                break;
            }
            if(can_close && glfwWindowShouldClose(window)) {
                goto close;
            }
            glfwPollEvents();
        }
    }

    u32 new_images_count = GPU_OPTIMAL_SWAPCHAIN_COUNT;
    u32 size_x           = surface_capabilities.currentExtent.width;
    u32 size_y           = surface_capabilities.currentExtent.height;

    if(surface_capabilities.maxImageCount == 0) {
        new_images_count = MAX(new_images_count, surface_capabilities.minImageCount);
    } else {
        new_images_count = CLAMP(surface_capabilities.minImageCount, surface_capabilities.maxImageCount, new_images_count);
    }

    VkSwapchainKHR new_swapchain = NULL;

    /* create swapchain */ {
        const VkSwapchainCreateInfoKHR swapchain_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .oldSwapchain     = old_swapchain,
            .surface          = surface,
            .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .minImageCount    = new_images_count,
            .imageColorSpace  = surface_color_space,
            .imageFormat      = surface_format,
            .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .imageArrayLayers = 1,
            .presentMode      = present_mode,
            .preTransform     = surface_capabilities.currentTransform,
            .imageExtent      = {size_x, size_y}
        };

        if(vkCreateSwapchainKHR(device, &swapchain_info, NULL, &new_swapchain) != VK_SUCCESS) {
            LOG_ERROR("failed to create swapchain");
            goto fail;
        }
        if(old_swapchain != NULL) {
            for(u32 i = 0; i != old_images_count; i++) {
                vkDestroyImageView(device, views[i], NULL);
                views [i] = NULL;
                images[i] = NULL;
            }
            vkDestroySwapchainKHR(device, old_swapchain, NULL);
        }
    }

    /* get images */ {
        vkGetSwapchainImagesKHR(device, new_swapchain, &new_images_count, NULL);
        if(new_images_count > GPU_MAX_SWAPCHAIN_IMAGES) {
            LOG_ERROR("too many swapchain images");
            goto fail;
        }
        vkGetSwapchainImagesKHR(device, new_swapchain, &new_images_count, images);

        for(u32 i = 0; i != new_images_count; i++) {
            const VkImageViewCreateInfo view_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .image    = images[i],
                .format   = surface_format,
                .components = {
                    .r = VK_COMPONENT_SWIZZLE_R,
                    .g = VK_COMPONENT_SWIZZLE_G,
                    .b = VK_COMPONENT_SWIZZLE_B,
                    .a = VK_COMPONENT_SWIZZLE_A
                },
                .subresourceRange = {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseArrayLayer = 0,
                    .baseMipLevel   = 0,
                    .layerCount     = 1,
                    .levelCount     = 1
                }
            };

            if(vkCreateImageView(device, &view_info, NULL, &views[i]) != VK_SUCCESS) {
                LOG_ERROR("failed to create swapchain image view");
                goto fail;
            }
        }
    }

    *swapchain    = new_swapchain;
    *swapchain_x  = size_x;
    *swapchain_y  = size_y;
    *images_count = new_images_count;
    return GPU_RESULT_SUCCESS;

    close: {
        return GPU_RESULT_CLOSE;
    }
    fail: {
        return GPU_RESULT_FAIL;
    }
}

b32 create_presistent_resources(const GpuVulkanObjects* vulkan_objects, const GpuVulkanDevice* vulkan_device, u32 images_max_count, GpuPersistentResources* persistent_resources) {    
    VkDevice device = vulkan_device->device;
    images_max_count = MAX(1, images_max_count);

    *persistent_resources = (GpuPersistentResources){0};
    /* create swapchain & swapchain images */ {
        if(create_swapchain(
            vulkan_objects, 
            vulkan_device, 
            &persistent_resources->swapchain,
            persistent_resources->swapchain_images,
            persistent_resources->swapchain_images_views,
            &persistent_resources->swapchain_images_count,
            &persistent_resources->swapchain_width,
            &persistent_resources->swapchain_height,
            FALSE
        ) != GPU_RESULT_SUCCESS) {
            LOG_ERROR("failed to create swapchain");
            goto fail;
        }
    }

    /* create samplers */ {
        const VkSamplerCreateInfo linear_repeat_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter               = VK_FILTER_LINEAR,
            .minFilter               = VK_FILTER_LINEAR,
            .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias              = 0.0f,
            .anisotropyEnable        = VK_FALSE,
            .maxAnisotropy           = 1.0f,
            .compareEnable           = VK_FALSE,
            .compareOp               = VK_COMPARE_OP_NEVER,
            .minLod                  = 0.0f,
            .maxLod                  = VK_LOD_CLAMP_NONE,
            .borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            .unnormalizedCoordinates = VK_FALSE
        };
        const VkSamplerCreateInfo linear_clamp_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter               = VK_FILTER_LINEAR,
            .minFilter               = VK_FILTER_LINEAR,
            .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias              = 0.0f,
            .anisotropyEnable        = VK_FALSE,
            .maxAnisotropy           = 1.0f,
            .compareEnable           = VK_FALSE,
            .compareOp               = VK_COMPARE_OP_NEVER,
            .minLod                  = 0.0f,
            .maxLod                  = VK_LOD_CLAMP_NONE,
            .borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            .unnormalizedCoordinates = VK_FALSE
        };
        const VkSamplerCreateInfo nearest_repeat_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter               = VK_FILTER_NEAREST,
            .minFilter               = VK_FILTER_NEAREST,
            .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias              = 0.0f,
            .anisotropyEnable        = VK_FALSE,
            .maxAnisotropy           = 1.0f,
            .compareEnable           = VK_FALSE,
            .compareOp               = VK_COMPARE_OP_NEVER,
            .minLod                  = 0.0f,
            .maxLod                  = VK_LOD_CLAMP_NONE,
            .borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            .unnormalizedCoordinates = VK_FALSE
        };
        const VkSamplerCreateInfo nearest_clamp_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter               = VK_FILTER_NEAREST,
            .minFilter               = VK_FILTER_NEAREST,
            .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias              = 0.0f,
            .anisotropyEnable        = VK_FALSE,
            .maxAnisotropy           = 1.0f,
            .compareEnable           = VK_FALSE,
            .compareOp               = VK_COMPARE_OP_NEVER,
            .minLod                  = 0.0f,
            .maxLod                  = VK_LOD_CLAMP_NONE,
            .borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            .unnormalizedCoordinates = VK_FALSE
        };

        if(vkCreateSampler(device, &linear_repeat_info, NULL, &persistent_resources->sampler_linear_repeat) != VK_SUCCESS) {
            LOG_ERROR("failed to create linear repeat sampler");
            goto fail;
        }
        if(vkCreateSampler(device, &linear_clamp_info, NULL, &persistent_resources->sampler_linear_clamp) != VK_SUCCESS) {
            LOG_ERROR("failed to create linear clamp sampler");
            goto fail;
        }
        if(vkCreateSampler(device, &nearest_repeat_info, NULL, &persistent_resources->sampler_nearest_repeat) != VK_SUCCESS) {
            LOG_ERROR("failed to create nearest repeat sampler");
            goto fail;
        }
        if(vkCreateSampler(device, &nearest_clamp_info, NULL, &persistent_resources->sampler_nearest_clamp) != VK_SUCCESS) {
            LOG_ERROR("failed to create nearest clamp sampler");
            goto fail;
        }
    }

    /* descriptors */ {
        const VkDescriptorPoolSize descriptor_pool_sizes[] = {
            (VkDescriptorPoolSize) {
                .type = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = 4
            },
            (VkDescriptorPoolSize) {
                .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = images_max_count
            },
            (VkDescriptorPoolSize) {
                .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = images_max_count
            }
        };
        const VkDescriptorSetLayoutBinding set_layout_bindings[] = {
            (VkDescriptorSetLayoutBinding) {
                .binding         = 0,
                .stageFlags      = VK_SHADER_STAGE_ALL,
                .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = 4
            },
            (VkDescriptorSetLayoutBinding) {
                .binding         = 1,
                .stageFlags      = VK_SHADER_STAGE_ALL,
                .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = images_max_count
            },
            (VkDescriptorSetLayoutBinding) {
                .binding         = 2,
                .stageFlags      = VK_SHADER_STAGE_ALL,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = images_max_count
            }
        };
        const VkDescriptorBindingFlags set_layout_bindings_flags[] = {
            0,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
        };

        /* descriptor pool */
        const VkDescriptorPoolCreateInfo descritptor_pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            .maxSets       = 1,
            .poolSizeCount = ARRAY_SIZE(descriptor_pool_sizes),
            .pPoolSizes    = descriptor_pool_sizes
        };
        if(vkCreateDescriptorPool(device, &descritptor_pool_info, NULL, &persistent_resources->descriptor_pool) != VK_SUCCESS) {
            LOG_ERROR("failed to create descriptor pool");
            goto fail;
        }

        /* set layout */
        const VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .bindingCount  = ARRAY_SIZE(set_layout_bindings_flags),
            .pBindingFlags = set_layout_bindings_flags
        };
        const VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext        = (void*)&binding_flags_info,
            .flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            .bindingCount = ARRAY_SIZE(set_layout_bindings),
            .pBindings    = set_layout_bindings
        };
        
        if(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_info, NULL, &persistent_resources->descriptor_set_layout) != VK_SUCCESS) {
            LOG_ERROR("failed to create descriptor set layout");
            goto fail;
        }

        /* descriptor set */
        const VkDescriptorSetAllocateInfo descriptor_set_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = persistent_resources->descriptor_pool,
            .pSetLayouts        = &persistent_resources->descriptor_set_layout,
            .descriptorSetCount = 1
        };

        if(vkAllocateDescriptorSets(device, &descriptor_set_info, &persistent_resources->descriptor_set) != VK_SUCCESS) {
            LOG_ERROR("failed to allocate descriptors set");
            goto fail;
        }

        /* pipeline layout */
        const VkPushConstantRange push_constants_range = {
            .offset     = 0,
            .size       = GPU_MAX_PUSH_CONSTANTS,
            .stageFlags = VK_SHADER_STAGE_ALL
        };
        const VkPipelineLayoutCreateInfo pipeline_layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = 1,
            .pSetLayouts            = &persistent_resources->descriptor_set_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &push_constants_range
        };

        if(vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, &persistent_resources->pipeline_layout) != VK_SUCCESS) {
            LOG_ERROR("failed to create pipeline layout");
            goto fail;
        }
    }

    /* write descriptors */ {
        const VkDescriptorImageInfo sampler_infos[] = {
            (VkDescriptorImageInfo) {
                .sampler = persistent_resources->sampler_linear_repeat
            },
            (VkDescriptorImageInfo) {
                .sampler = persistent_resources->sampler_linear_clamp
            },
            (VkDescriptorImageInfo) {
                .sampler = persistent_resources->sampler_nearest_repeat
            },
            (VkDescriptorImageInfo) {
                .sampler = persistent_resources->sampler_nearest_clamp
            }
        };

        const VkWriteDescriptorSet descriptor_writes[] = {
            (VkWriteDescriptorSet) {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = persistent_resources->descriptor_set,
                .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER,
                .dstBinding      = GPU_BIDNING_SAMPLERS,
                .descriptorCount = 1,
                .dstArrayElement = 0,
                .pImageInfo      = &sampler_infos[0]
            },
            (VkWriteDescriptorSet) {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = persistent_resources->descriptor_set,
                .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER,
                .dstBinding      = GPU_BIDNING_SAMPLERS,
                .descriptorCount = 1,
                .dstArrayElement = 1,
                .pImageInfo      = &sampler_infos[1]
            },
            (VkWriteDescriptorSet) {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = persistent_resources->descriptor_set,
                .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER,
                .dstBinding      = GPU_BIDNING_SAMPLERS,
                .descriptorCount = 1,
                .dstArrayElement = 2,
                .pImageInfo      = &sampler_infos[2]
            },
            (VkWriteDescriptorSet) {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = persistent_resources->descriptor_set,
                .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER,
                .dstBinding      = GPU_BIDNING_SAMPLERS,
                .descriptorCount = 1,
                .dstArrayElement = 3,
                .pImageInfo      = &sampler_infos[3]
            }
        };

        vkUpdateDescriptorSets(device, ARRAY_SIZE(descriptor_writes), descriptor_writes, 0, NULL);
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

void destroy_persistent_resources(const GpuVulkanDevice* vulkan_device, GpuPersistentResources* persistent_resources) {
    VkDevice device = vulkan_device->device;

    VkSwapchainKHR swapchain              = persistent_resources->swapchain;
    VkImageView*   swapchain_views        = persistent_resources->swapchain_images_views;
    u32            swapchain_images_count = persistent_resources->swapchain_images_count;

    for(u32 i = 0; i != swapchain_images_count; i++) {
        vkDestroyImageView(device, swapchain_views[i], NULL);
    }
    vkDestroySwapchainKHR(device, swapchain, NULL);

    VkDescriptorPool      descriptor_pool       = persistent_resources->descriptor_pool;
    VkDescriptorSetLayout descriptor_set_layout = persistent_resources->descriptor_set_layout;
    VkPipelineLayout      pipeline_layout       = persistent_resources->pipeline_layout;
    vkDestroyDescriptorPool(device, descriptor_pool, NULL);
    vkDestroyPipelineLayout(device, pipeline_layout, NULL);
    vkDestroyDescriptorSetLayout(device, descriptor_set_layout, NULL);

    VkSampler sampler_linear_repeat  = persistent_resources->sampler_linear_repeat;
    VkSampler sampler_linear_clamp   = persistent_resources->sampler_linear_clamp;
    VkSampler sampler_nearest_repeat = persistent_resources->sampler_nearest_repeat;
    VkSampler sampler_nearest_clamp  = persistent_resources->sampler_nearest_clamp;
    vkDestroySampler(device, sampler_linear_repeat, NULL);
    vkDestroySampler(device, sampler_linear_clamp, NULL);
    vkDestroySampler(device, sampler_nearest_repeat, NULL);
    vkDestroySampler(device, sampler_nearest_clamp, NULL);

    *persistent_resources = (GpuPersistentResources){0};
}

/* === sync objects === */

b32 create_sync_objects(const GpuVulkanDevice* vulkan_device, GpuSyncObjects* sync_objects) {
    VkDevice       device = vulkan_device->device;
    GpuSyncObjects sync   = (GpuSyncObjects){0};

    const VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    const VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    
    /* render queue */ {
        if(vkCreateFence(device, &fence_info, NULL, &sync.render_fence) != VK_SUCCESS) {
            LOG_ERROR("failed to create render fence");
            goto fail;
        }
        if(vkCreateSemaphore(device, &semaphore_info, NULL, &sync.image_acquire_semaphore) != VK_SUCCESS) {
            LOG_ERROR("failed to create image acquire semaphore");
            goto fail;
        }
        for(u32 i = 0; i != GPU_MAX_SWAPCHAIN_IMAGES; i++) {
            if(vkCreateSemaphore(device, &semaphore_info, NULL, &sync.image_submit_semaphores[i]) != VK_SUCCESS) {
                LOG_ERROR("failed to create image submit semaphores");
                goto fail;
            }
        }
    }

    /* transfer queue */ {
        if(vulkan_device->command_buffer_transfer != NULL) {
            if(vkCreateFence(device, &fence_info, NULL, &sync.transfer_fence) != VK_SUCCESS) {
                LOG_ERROR("failed to create transfer fence");
                goto fail;
            }
        }
    }

    *sync_objects = sync;
    return TRUE;

    fail: {
        return FALSE;
    }
}

void destroy_sync_objects(const GpuVulkanDevice* vulkan_device, GpuSyncObjects* sync_objects) {
    VkDevice device = vulkan_device->device;
    vkDestroyFence(device, sync_objects->render_fence, NULL);
    vkDestroySemaphore(device, sync_objects->image_acquire_semaphore, NULL);
    for(u32 i = 0; i != GPU_MAX_SWAPCHAIN_IMAGES; i++) {
        vkDestroySemaphore(device, sync_objects->image_submit_semaphores[i], NULL);
    }

    if(vulkan_device->command_buffer_transfer != NULL) {
        vkDestroyFence(device, sync_objects->transfer_fence, NULL);
    }
    *sync_objects = (GpuSyncObjects){0};
}

/* === pipelines === */
/* FIX: move stuff like culling into dynamic state */

b32 compile_shader_module(VkDevice device, const char* name, VkShaderModule* shader_module, void** read_buffer, u64* read_buffer_size) {
    FILE* file = fopen(name, "rb");
    if(file == NULL) {
        LOG_ERROR("failed to open shader file: %s", name);
        goto fail;
    }

    u64 file_size = 0;

    /* file size (reallocation of buffer) */ {
        _fseeki64(file, 0, SEEK_END);
        file_size = (u64)_ftelli64(file);

        if(file_size == 0) {
            LOG_ERROR("empty shader file: %s", name);
            goto fail;
        }

        if(file_size > *read_buffer_size) {
            void* new_buffer = realloc(*read_buffer, file_size);
            if(new_buffer == NULL) {
                LOG_ERROR("failed to reallocate read buffer");
                goto fail;
            }

            *read_buffer = new_buffer;
            *read_buffer_size = file_size;
        }
    }

    /* read file */ {
        u64 file_size_remain = file_size;
        u64 file_size_read   = 0;
        fseek(file, 0, SEEK_SET);
        while(1) {
            u64 read = fread((u8*)*read_buffer + file_size_read, 1, file_size_remain, file);
            file_size_read   += read;
            file_size_remain -= read;
            if(file_size_remain == 0) {
                break;
            }
            if(read == 0) {
                LOG_ERROR("failed to read shader file: %s", name);
                goto fail;
            }
        }
    }

    const VkShaderModuleCreateInfo module_info = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pCode    = *read_buffer,
        .codeSize = file_size
    };
    if(vkCreateShaderModule(device, &module_info, NULL, shader_module) != VK_SUCCESS) {
        LOG_ERROR("failed to create shader module from file: %s", name);
        goto fail;
    }

    fclose(file);
    return TRUE;

    fail: {
        if(file != NULL) {
            fclose(file);
        }
        return FALSE;
    }
}

b32 create_compute_pipeline(
    VkDevice         device, 
    VkPipelineLayout pipeline_layout, 
    GpuPipelineFlags flags, 
    VkShaderModule   compute_shader,
    GpuPipeline*     gpu_pipeline
) {
    const VkComputePipelineCreateInfo compute_pipeline_info = {
        .sType              = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout             = pipeline_layout,
        .basePipelineHandle = NULL,
        .basePipelineIndex  = -1,
        .stage              = {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
            .pName  = GPU_COMPUTE_SHADER_ENTRY,
            .module = compute_shader
        }
    };

    VkPipeline compute_pipeline = NULL;

    if(vkCreateComputePipelines(device, NULL, 1, &compute_pipeline_info, NULL, &compute_pipeline) != VK_SUCCESS) {
        LOG_ERROR("failed to create compute pipeline");
        goto fail;
    }

    *gpu_pipeline = (GpuPipeline) {
        .pipeline_flags = flags,
        .pipeline       = compute_pipeline
    };
    return TRUE;

    fail: {
        return FALSE;
    }
}

b32 create_graphics_pipeline(
    VkDevice         device,
    VkPipelineLayout pipeline_layout, 
    GpuPipelineFlags flags, 
    VkShaderModule   vertex_shader, 
    VkShaderModule   fragment_shader, 
    const VkFormat*  color_formats, 
    u32              color_formats_count, 
    VkFormat         depth_format,
    GpuPipeline*     gpu_pipeline
) {
    /* create pipeline */
    const VkPipelineShaderStageCreateInfo shader_stages[2] = {
        (VkPipelineShaderStageCreateInfo) {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName  = GPU_VERTEX_SHADER_ENTRY,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_shader
        },
        (VkPipelineShaderStageCreateInfo) {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName  = GPU_FRAGMENT_SHADER_ENTRY,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragment_shader
        }
    };

    /* rendering info */
    const VkPipelineRenderingCreateInfoKHR rendering_create_info = (VkPipelineRenderingCreateInfoKHR) {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount    = color_formats_count,
        .pColorAttachmentFormats = color_formats,
        .depthAttachmentFormat   = depth_format
    };

    /* dynamic states */
    const VkDynamicState graphics_pipeline_dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    const VkPipelineDynamicStateCreateInfo dynamic_state = (VkPipelineDynamicStateCreateInfo) {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ARRAY_SIZE(graphics_pipeline_dynamic_states),
        .pDynamicStates    = graphics_pipeline_dynamic_states
    };
    const VkPipelineViewportStateCreateInfo viewport_state = (VkPipelineViewportStateCreateInfo) {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1
    };

    /* static states */
    const VkPipelineVertexInputStateCreateInfo vertex_input_state = (VkPipelineVertexInputStateCreateInfo) {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexAttributeDescriptionCount = 0,
        .vertexBindingDescriptionCount   = 0,
        .pVertexAttributeDescriptions    = NULL,
        .pVertexBindingDescriptions      = NULL
    };
    const VkPipelineInputAssemblyStateCreateInfo input_assembly_state = (VkPipelineInputAssemblyStateCreateInfo) {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = FALSE
    };
    const VkPipelineRasterizationStateCreateInfo rasterization_state = (VkPipelineRasterizationStateCreateInfo) {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = FALSE,
        .rasterizerDiscardEnable = FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .lineWidth               = 1.0f,
        .cullMode                = VK_CULL_MODE_NONE,
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = FALSE,
        .depthBiasConstantFactor = 0.0,
        .depthBiasClamp          = 0.0,
        .depthBiasSlopeFactor    = 0.0
    };
    const VkPipelineMultisampleStateCreateInfo multisample_state = (VkPipelineMultisampleStateCreateInfo) {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable   = FALSE,
        .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading      = 1.0f,
        .pSampleMask           = NULL,
        .alphaToCoverageEnable = FALSE,
        .alphaToOneEnable      = FALSE
    };
    const VkPipelineColorBlendAttachmentState color_blend_attachment_state = (VkPipelineColorBlendAttachmentState) {
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable         = FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD
    };
    const VkPipelineColorBlendStateCreateInfo color_blend_state = (VkPipelineColorBlendStateCreateInfo) {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable     = VK_FALSE,
        .logicOp           = VK_LOGIC_OP_COPY,
        .attachmentCount   = 1,
        .pAttachments      = &color_blend_attachment_state,
        .blendConstants[0] = 0.0f,
        .blendConstants[1] = 0.0f,
        .blendConstants[2] = 0.0f,
        .blendConstants[3] = 0.0f
    };
    const VkPipelineDepthStencilStateCreateInfo depth_stencil_state = (VkPipelineDepthStencilStateCreateInfo) {
        .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable  = TRUE,
        .depthWriteEnable = TRUE,
        .depthCompareOp   = VK_COMPARE_OP_GREATER_OR_EQUAL
    };

    VkGraphicsPipelineCreateInfo graphics_pipeline_info = (VkGraphicsPipelineCreateInfo) {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .basePipelineHandle  = NULL,
        .basePipelineIndex   = -1,
        .stageCount          = 2,
        .pStages             = shader_stages,
        .pVertexInputState   = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pViewportState      = &viewport_state,
        .pRasterizationState = &rasterization_state,
        .pMultisampleState   = &multisample_state,
        .pDepthStencilState  = &depth_stencil_state,
        .pColorBlendState    = &color_blend_state,
        .pDynamicState       = &dynamic_state,
        .layout              = pipeline_layout,
        .renderPass          = NULL,
        .pNext               = &rendering_create_info
    };

    VkPipeline graphics_pipeline = NULL;

    if(vkCreateGraphicsPipelines(device, NULL, 1, &graphics_pipeline_info, NULL, &graphics_pipeline) != VK_SUCCESS) {
        LOG_ERROR("failed to create graphics pipeline");
        goto fail;
    }

    *gpu_pipeline = (GpuPipeline) {
        .pipeline_flags = flags,
        .pipeline       = graphics_pipeline
    };
    return TRUE;

    fail: {
        return FALSE;
    }
}

b32 create_pipelines(const GpuVulkanDevice* vulkan_device, const GpuPersistentResources* persistent_resources, const GpuPipelineInfo* pipelines_infos, u32 pipelines_count, GpuPipelines* gpu_pipelines) {
    VkDevice         device          = vulkan_device->device;
    VkPipelineLayout pipeline_layout = persistent_resources->pipeline_layout;
    VkFormat         surface_format  = vulkan_device->surface_format;

    /* allocate pipelines array */
    GpuPipeline* pipelines = calloc(pipelines_count, sizeof(GpuPipeline));
    if(pipelines == NULL) {
        LOG_ERROR("failed to allocate pipelines array");
        goto fail;
    }

    void* read_buffer      = NULL;
    u64   read_buffer_size = 0;

    /* compile pipelines */
    for(u32 i = 0; i != pipelines_count; i++) {
        if(pipelines_infos[i].flags & GPU_PIPELINE_FLAG_COMPUTE) {
            VkShaderModule compute_module = NULL;
            
            if(!compile_shader_module(
                device,
                pipelines_infos[i].compute_shader,
                &compute_module,
                &read_buffer,
                &read_buffer_size
            )) {
                LOG_ERROR("failed to compile compute shader module id: %u", i);
                goto fail;
            }
            
            /* create pipeline */
            if(!create_compute_pipeline(
                device, 
                pipeline_layout, 
                pipelines_infos[i].flags, 
                compute_module,
                &pipelines[i]
            )) {
                LOG_ERROR("failed to create compute pipeline id: %u", i);
                goto fail;
            }

            vkDestroyShaderModule(device, compute_module, NULL);
        } else {
            VkShaderModule vertex_module   = NULL;
            VkShaderModule fragment_module = NULL;

            if(!compile_shader_module(
                device,
                pipelines_infos[i].vertex_shader,
                &vertex_module,
                &read_buffer,
                &read_buffer_size
            )) {
                LOG_ERROR("failed to compile vertex shader module id: %u", i);
                goto fail;
            }
            
            if(!compile_shader_module(
                device, 
                pipelines_infos[i].fragment_shader,
                &fragment_module,
                &read_buffer,
                &read_buffer_size
            )) {
                LOG_ERROR("failed to compile fragment shader module id: %u", i);
                goto fail;
            }

            u32      color_formats_count                = pipelines_infos[i].color_formats_count;
            VkFormat color_formats[GPU_MAX_ATTACHMENTS] = {0};
            VkFormat depth_format                       = VK_FORMAT_UNDEFINED;

            /* convert formats */ {
                if(color_formats_count > GPU_MAX_ATTACHMENTS) {
                    LOG_ERROR("too many color attachements on pipeline id: %u", i);
                    goto fail;
                }
                for(u32 k = 0; k != color_formats_count; k++) {
                    color_formats[k] = gpu_convert_format(surface_format, pipelines_infos[i].color_formats[k], NULL);
                }
                depth_format = gpu_convert_format(surface_format, pipelines_infos[i].depth_format, NULL);
            }

            /* create pipeline */
            if(!create_graphics_pipeline(
                device,
                pipeline_layout, 
                pipelines_infos[i].flags, 
                vertex_module, 
                fragment_module,
                color_formats, 
                color_formats_count, 
                depth_format,
                &pipelines[i]
            )) {
                LOG_ERROR("failed to create graphics pipeline id: %u", i);
            }

            vkDestroyShaderModule(device, vertex_module, NULL);
            vkDestroyShaderModule(device, fragment_module, NULL);
        }
    }

    if(read_buffer != NULL) {
        free(read_buffer);
        read_buffer_size = 0;
    }

    *gpu_pipelines = (GpuPipelines) {
        .pipelines       = pipelines,
        .pipelines_count = pipelines_count
    };
    return TRUE;

    fail: {
        return FALSE;
    }
}

void destroy_pipelines(const GpuVulkanDevice* vulkan_device, GpuPipelines* gpu_pipelines) {
    VkDevice     device          = vulkan_device->device;
    GpuPipeline* pipelines       = gpu_pipelines->pipelines;
    u32          pipelines_count = gpu_pipelines->pipelines_count;

    for(u32 i = 0; i != pipelines_count; i++) {
        vkDestroyPipeline(device, pipelines[i].pipeline, NULL);
        pipelines[i] = (GpuPipeline){0};
    }

    free(pipelines);
    *gpu_pipelines = (GpuPipelines){0};
}

/* === images === */

b32 create_images_pool(GpuImagesPool* images_pool, u32 images_max_count) {
    if(images_max_count == 0) {
        *images_pool = (GpuImagesPool){0};
        goto success;
    }

    void* allocation = malloc((sizeof(GpuImage) + sizeof(GpuImageHandle)) * images_max_count);
    if(allocation == NULL) {
        LOG_ERROR("failed to allocate images pool");
        goto fail;
    }

    GpuImage*       images            = (void*)((u8*)allocation);
    GpuImageHandle* images_occupation = (void*)((u8*)allocation + sizeof(GpuImage) * images_max_count);

    memset(images, 0, sizeof(GpuImage) * images_max_count);
    for(u32 i = 0; i != images_max_count; i++) {
        images_occupation[i] = (GpuImageHandle)i;
    }

    *images_pool = (GpuImagesPool) {
        .allocation        = allocation,
        .images            = images,
        .images_occupation = images_occupation,
        .images_count      = 0,
        .images_max_count  = images_max_count
    };

    success: {
        return TRUE;
    }

    fail: {
        return FALSE;
    }
}

void destroy_images_pool(const GpuVulkanDevice* vulkan_device, GpuImagesPool* images_pool) {
    VkDevice  device       = vulkan_device->device; 
    GpuImage* images       = images_pool->images;
    u32       images_count = images_pool->images_count;

    for(u32 i = 0; i != images_count; i++) {
        vkDestroyImageView(device, images[i].view, NULL);
        vkDestroyImage(device, images[i].image, NULL);
        images[i] = (GpuImage){0};
    }

    free(images_pool->allocation);
    *images_pool = (GpuImagesPool){0};
}

/* === local === */

GpuResult gpu_recreate_swapchain(GpuContext* context) {
    GpuVulkanObjects*       vulkan_objects       = &context->vulkan_objects;
    GpuVulkanDevice*        vulkan_device        = &context->vulkan_device;
    GpuPersistentResources* persistent_resources = &context->persistent_resources;

    GpuResult result = create_swapchain(
        vulkan_objects, 
        vulkan_device, 
        &persistent_resources->swapchain,
        persistent_resources->swapchain_images,
        persistent_resources->swapchain_images_views,
        &persistent_resources->swapchain_images_count,
        &persistent_resources->swapchain_width,
        &persistent_resources->swapchain_height,
        TRUE
    );

    if(result == GPU_RESULT_FAIL) {
        LOG_ERROR("f y");
    }

    return result;
}

/* === interface === */

GpuContext* gpu_init(const GpuInitInfo* init_info) {
    if( init_info == NULL ||
        init_info->window_height == 0 ||
        init_info->window_width  == 0
    ) {
        LOG_ERROR("invalid input");
        goto fail;
    }

    /* allocate context on heap */
    GpuContext* gpu_ctx = calloc(1, sizeof(GpuContext));
    if(gpu_ctx == NULL) {
        LOG_ERROR("failed to allocate gpu context");
        goto fail;
    }
    *gpu_ctx = (GpuContext){0};

    /* create vulkan window, instance, etc. */
    if(!create_vulkan_objects(
        init_info->window_name, 
        init_info->window_width, 
        init_info->window_height, 
        init_info->config_flags, 
        &gpu_ctx->vulkan_objects
    )) {
        LOG_ERROR("failed to create vulkan objects");
        goto fail;
    }

    /* create vulkan device, queues etc. */
    if(!create_vulkan_device(
        init_info->config_flags,
        init_info->pci_vendor_device,
        init_info->malloc_heap_size + init_info->images_heap_size,
        &gpu_ctx->vulkan_objects,
        &gpu_ctx->vulkan_device
    )) {
        LOG_ERROR("failed to create vulkan device");
        goto fail;
    }

    if(!create_memory_pools(
        init_info->malloc_heap_size,
        init_info->images_heap_size,
        &gpu_ctx->vulkan_device,
        &gpu_ctx->memory_pools
    )) {
        LOG_ERROR("failed to create memory pools");
        goto fail;
    }

    if(!create_presistent_resources(
        &gpu_ctx->vulkan_objects,
        &gpu_ctx->vulkan_device,
        init_info->images_max_count,
        &gpu_ctx->persistent_resources
    )) {
        LOG_ERROR("failed to create persistent resources");
        goto fail;
    }

    if(!create_sync_objects(
        &gpu_ctx->vulkan_device,
        &gpu_ctx->sync_objects
    )) {
        LOG_ERROR("failed to create sync objects");
        goto fail;
    }

    if(!create_images_pool(&gpu_ctx->images_pool, init_info->images_max_count)) {
        LOG_ERROR("failed to create images pool");
        goto fail;
    }

    return gpu_ctx;

    fail: {
        return NULL;
    }
}

void gpu_terminate(GpuContext* context) {
    vkDeviceWaitIdle(context->vulkan_device.device);

    destroy_sync_objects(&context->vulkan_device, &context->sync_objects);
    
    destroy_images_pool(&context->vulkan_device, &context->images_pool);
    destroy_pipelines(&context->vulkan_device, &context->pipelines);
    destroy_persistent_resources(&context->vulkan_device, &context->persistent_resources);
    destroy_memory_pools(&context->vulkan_device, &context->memory_pools);
    destroy_vulkan_device(&context->vulkan_objects, &context->vulkan_device);
    destroy_vulkan_objects(&context->vulkan_objects);

    *context = (GpuContext){0};
    free(context);
}

b32 gpu_compile_pipelines(GpuContext* context, const GpuPipelineInfo* pipeline_infos, u32 pipelines_count) {
    if(!create_pipelines(
        &context->vulkan_device, 
        &context->persistent_resources, 
        pipeline_infos, 
        pipelines_count, 
        &context->pipelines
    )) {
        LOG_ERROR("failed to compile pipelines");
        goto fail;
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

u64 gpu_malloc(GpuContext* context, u64 size, u64 alignment) {
    GpuMemoryPools* memory_pools = &context->memory_pools;
    u64  buffer_address     = memory_pools->malloc_buffer_address;
    u64* pages_malloc_bits  = memory_pools->pages_malloc_bits;
    u64  pages_malloc_count = memory_pools->pages_malloc_count;

    u64 offset = allocate_memory_allocator(pages_malloc_bits, pages_malloc_count, size, alignment);
    if(offset == U64_MAX) {
        LOG_ERROR("failed to allocate gpu malloc memory");
        goto fail;
    }

    return buffer_address + offset;

    fail: {
        return U64_MAX;
    }
}

void gpu_free(GpuContext* context, u64 address, u64 size) {
    GpuMemoryPools* memory_pools = &context->memory_pools;

    u64  buffer_address     = memory_pools->malloc_buffer_address;
    u64* pages_malloc_bits  = memory_pools->pages_malloc_bits;
    u64  pages_malloc_count = memory_pools->pages_malloc_count;

    u64 offset = address - buffer_address;

    if(!free_memory_allocator(pages_malloc_bits, pages_malloc_count, offset, size)) {
        LOG_ERROR("failed to free gpu malloc memory");
    }
}

GpuImageHandle gpu_add_image(GpuContext* context, const GpuImageInfo* image_info) {
    GpuImagesPool*  images_pool    = &context->images_pool;
    GpuMemoryPools* memory_pools   = &context->memory_pools;
    VkDevice        device         = context->vulkan_device.device;
    VkDescriptorSet descriptor_set = context->persistent_resources.descriptor_set;

    GpuImage*       images            = images_pool->images;
    GpuImageHandle* images_occupation = images_pool->images_occupation;
    u32             old_images_count  = images_pool->images_count;
    u32             max_images_count  = images_pool->images_max_count;

    GpuImageHandle image_id = GPU_INVALID_HANDLE;

    /* find slot */ {
        if(old_images_count >= max_images_count) {
            LOG_ERROR("not enough slots for image");
            goto no_slot_fail;
        }
        image_id = images_occupation[old_images_count];
        images_pool->images_count++;
    }

    const GpuImageFlags image_flags     = image_info->flags;
    const u32           image_width     = image_info->width;
    const u32           image_height    = image_info->height;
    const u32           image_mip_count = image_info->mip_count;

    VkImage            image        = NULL;
    VkImageAspectFlags image_aspect = VK_IMAGE_ASPECT_NONE;
    VkFormat           image_format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags  image_usage  = 0;

    /* convert format & usage */ {
        image_format = gpu_convert_format(VK_FORMAT_UNDEFINED, image_info->format, &image_aspect);
        image_usage = gpu_convert_image_usage(image_info->flags);
        
        if(image_format == VK_FORMAT_UNDEFINED) {
            LOG_ERROR("invalid image format");
            goto fail;
        }
        if(image_usage == 0) {
            LOG_ERROR("invalid image usage");
            goto fail;
        }
    }

    /* create image */ {
        const VkImageCreateInfo image_create_info = {
            .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType   = VK_IMAGE_TYPE_2D,
            .usage       = image_usage,
            .format      = image_format,
            .extent      = {image_width, image_height, 1},
            .arrayLayers = 1,
            .mipLevels   = image_mip_count,
            .samples     = VK_SAMPLE_COUNT_1_BIT,
            .tiling      = VK_IMAGE_TILING_OPTIMAL
        };

        if(vkCreateImage(device, &image_create_info, NULL, &image) != VK_SUCCESS) {
            LOG_ERROR("failed to create image");
            goto fail;
        }
    }

    u64            memory_offset = 0;
    u64            memory_size   = 0;
    VkImageView    image_view    = NULL;

    /* bind & create view */ {
        VkDeviceMemory images_memory = memory_pools->memory_images;
        u64* images_pages_bits  = memory_pools->pages_images_bits;
        u64  images_pages_count = memory_pools->pages_images_count;

        VkMemoryRequirements memory_requirements = (VkMemoryRequirements){0};
        vkGetImageMemoryRequirements(device, image, &memory_requirements);

        memory_size   = memory_requirements.size;
        memory_offset = allocate_memory_allocator(images_pages_bits, images_pages_count, memory_requirements.size, memory_requirements.alignment);
        if(memory_offset == U64_MAX) {
            LOG_ERROR("failed to allocate image");
            goto fail;
        }

        if(vkBindImageMemory(device, image, images_memory, memory_offset) != VK_SUCCESS) {
            LOG_ERROR("failed to bind image memory");
            goto fail;
        }

        const VkImageViewCreateInfo image_view_create_info = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = image,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = image_format,
            .components       = {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_G,
                .b = VK_COMPONENT_SWIZZLE_B,
                .a = VK_COMPONENT_SWIZZLE_A
            },
            .subresourceRange = {
                .aspectMask     = image_aspect,
                .baseArrayLayer = 0,
                .baseMipLevel   = 0,
                .layerCount     = 1,
                .levelCount     = image_mip_count
            }
        };

        if(vkCreateImageView(device, &image_view_create_info, NULL, &image_view) != VK_SUCCESS) {
            LOG_ERROR("failed to create image view");
            goto fail;
        }
    }

    /* update descriptors */ {
        VkDescriptorImageInfo descriptor_image_infos[2] = {0};
        VkWriteDescriptorSet  descriptor_writes[2]      = {0};
        u32                   descriptor_writes_count   = 0;

        /* sampled descriptor */
        if(image_usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
            descriptor_image_infos[descriptor_writes_count] = (VkDescriptorImageInfo) {
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .imageView   = image_view
            };
            descriptor_writes[descriptor_writes_count] = (VkWriteDescriptorSet) {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = 1,
                .dstSet          = descriptor_set,
                .dstBinding      = GPU_BINDING_SAMPLED_IMAGES,
                .dstArrayElement = image_id,
                .pImageInfo      = &descriptor_image_infos[descriptor_writes_count]
            };
            descriptor_writes_count++;
        }
        /* storage descriptor */
        if(image_usage & VK_IMAGE_USAGE_STORAGE_BIT) {
            descriptor_image_infos[descriptor_writes_count] = (VkDescriptorImageInfo) {
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                .imageView   = image_view
            };
            descriptor_writes[descriptor_writes_count] = (VkWriteDescriptorSet) {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
                .dstSet          = descriptor_set,
                .dstBinding      = GPU_BINDING_STORAGE_IMAGES,
                .dstArrayElement = image_id,
                .pImageInfo      = &descriptor_image_infos[descriptor_writes_count]
            };
            descriptor_writes_count++;
        }

        vkUpdateDescriptorSets(device, descriptor_writes_count, descriptor_writes, 0, NULL);
    }

    images[image_id] = (GpuImage) {
        .flags          = image_flags,
        .format         = image_format,
        .aspect         = image_aspect,
        .width          = image_width,
        .height         = image_height,
        .mip_count      = image_mip_count,
        .memory_offset  = memory_offset,
        .memory_size    = memory_size,
        .image          = image,
        .view           = image_view,
        .layout         = VK_IMAGE_LAYOUT_UNDEFINED,
        .access         = VK_ACCESS_NONE,
        .pipeline_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
    };
    return image_id;

    no_slot_fail: {
        return GPU_INVALID_HANDLE;
    }
    fail: {
        images_pool->images_count--;
        return GPU_INVALID_HANDLE;
    }
}

void gpu_remove_image(GpuContext* context, GpuImageHandle image_id) {
    GpuImagesPool*  images_pool    = &context->images_pool;
    GpuMemoryPools* memory_pools   = &context->memory_pools;
    VkDevice        device         = context->vulkan_device.device;

    GpuImage*       images            = images_pool->images;
    GpuImageHandle* images_occupation = images_pool->images_occupation;
    u32             old_images_count  = images_pool->images_count;
    u32             max_images_count  = images_pool->images_max_count;

    if(image_id >= max_images_count) {
        LOG_ERROR("invalid image id");
        goto fail;
    }
    if(images[image_id].image == NULL) {
        LOG_ERROR("invalid image (already free)");
        goto fail;
    }

    GpuImage destroy_image = images[image_id];
    images[image_id]       = (GpuImage){0};
    b32 found              = FALSE;
    for(u32 i = 0; i != old_images_count; i++) {
        if(images_occupation[i] == image_id) {
            images_occupation[i] = images_occupation[old_images_count - 1];
            images_occupation[old_images_count - 1] = image_id;
            found = TRUE;
            break;
        }
    }

    if(!found) {
        LOG_ERROR("critical bug in images occupation system");
        goto fail;
    }

    images_pool->images_count--;

    vkDestroyImageView(device, destroy_image.view, NULL);
    vkDestroyImage(device, destroy_image.image, NULL);
    free_memory_allocator(memory_pools->pages_images_bits, memory_pools->pages_images_count, destroy_image.memory_offset, destroy_image.memory_size);

    fail: {}
}

/* ==== ==== ==== ==== ==== ==== ==== ==== ==== 
    commands
   ==== ==== ==== ==== ==== ==== ==== ==== ==== */
/* FIX: add u64 overflow checks on memory barrier 
   FIX: add u64 overflow checks sync memwrite
   FIX: might need to change memory barrier to handle multiple allocations */

void gpu_transit_image(VkCommandBuffer command_buffer, GpuImage* gpu_image, VkImageLayout dst_layout, VkAccessFlags dst_access, VkPipelineStageFlags dst_stage) {
    const u32                  mip_count    = gpu_image->mip_count;
    const VkImageLayout        src_layout   = gpu_image->layout;
    const VkAccessFlags        src_access   = gpu_image->access;
    const VkImageAspectFlags   image_aspect = gpu_image->aspect;
    const VkPipelineStageFlags src_stage    = gpu_image->pipeline_stage;
    const VkImage              image        = gpu_image->image;
    
    const VkImageMemoryBarrier image_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .image         = image,
        .oldLayout     = src_layout,
        .srcAccessMask = src_access,
        .newLayout     = dst_layout,
        .dstAccessMask = dst_access,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .subresourceRange = {
            .aspectMask     = image_aspect,
            .baseArrayLayer = 0,
            .layerCount     = 1,
            .baseMipLevel   = 0,
            .levelCount     = mip_count
        }
    };

    vkCmdPipelineBarrier(
        command_buffer,
        src_stage,
        dst_stage,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &image_barrier
    );

    gpu_image->pipeline_stage = dst_stage;
    gpu_image->layout = dst_layout;
    gpu_image->access = dst_access;
}

GpuResult gpu_cmd_screen_begin(GpuContext* context, u32* screen_x, u32* screen_y) {
    GpuVulkanDevice*        vulkan_device        = &context->vulkan_device;
    GpuPersistentResources* persistent_resources = &context->persistent_resources;

    VkDevice        device                  = vulkan_device->device;
    VkCommandBuffer command_buffer          = vulkan_device->command_buffer_render;
    VkFence         render_fence            = context->sync_objects.render_fence;
    VkSemaphore     image_acquire_semaphore = context->sync_objects.image_acquire_semaphore;

    if(vkWaitForFences(device, 1, &render_fence, TRUE, U64_MAX) != VK_SUCCESS) {
        LOG_ERROR("failed to wait for render fence");
        goto fail;
    }

    u32         render_image_id   = U32_MAX;
    VkImage     render_image      = NULL;
    VkImageView render_image_view = NULL;

    /* acquire image */ {
        reacquire_image: {
            VkResult acquire_result = vkAcquireNextImageKHR(
                device, 
                persistent_resources->swapchain, 
                U64_MAX, 
                image_acquire_semaphore, 
                NULL, 
                &render_image_id
            );
            /* resize swapchain */
            if(acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
                if(vkDeviceWaitIdle(device) != VK_SUCCESS) {
                    LOG_ERROR("failed to wait for device");
                    goto fail;
                }

                GpuResult swapchain_resize_result = gpu_recreate_swapchain(context);
                if(swapchain_resize_result == GPU_RESULT_FAIL) {
                    LOG_ERROR("failed to resize swapchain");
                    goto fail;
                }
                else if(swapchain_resize_result == GPU_RESULT_CLOSE) {
                    goto close;
                }
                else {  
                    goto reacquire_image;
                }
            }
            /* acquire error */
            else if(acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
                LOG_ERROR("failed to acquire image");
                goto fail;
            }

            if(vkResetFences(device, 1, &render_fence) != VK_SUCCESS) {
                LOG_ERROR("failed to reset render fence");
                goto fail;
            }
        }

        render_image      = persistent_resources->swapchain_images      [render_image_id];
        render_image_view = persistent_resources->swapchain_images_views[render_image_id];
    }

    /* begin command buffer */ {
        const VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };
        if(vkResetCommandBuffer(command_buffer, 0) != VK_SUCCESS) {
            LOG_ERROR("failed to reset render command buffer");
            goto fail;
        }
        if(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info) != VK_SUCCESS) {
            LOG_ERROR("failed to begin render command buffer");
            goto fail;
        }

        VkDescriptorSet  descriptor_set  = persistent_resources->descriptor_set;
        VkPipelineLayout pipeline_layout = persistent_resources->pipeline_layout; 

        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
    }

    /* top render image barrier */ {
        const VkImageMemoryBarrier render_image_barrier = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask    = VK_ACCESS_NONE,
            .dstAccessMask    = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            .oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .image            = render_image,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .subresourceRange = (VkImageSubresourceRange) {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        };
        vkCmdPipelineBarrier(
            command_buffer, 
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0,
            NULL,
            0,
            NULL,
            1,
            &render_image_barrier
        );
    }

    *screen_x = persistent_resources->swapchain_width;
    *screen_y = persistent_resources->swapchain_height;

    context->cmd = (GpuCmd) {
        .render_image_id   = render_image_id,
        .render_image      = render_image,
        .render_image_view = render_image_view
    };

    return GPU_RESULT_SUCCESS;

    close: {
        return GPU_RESULT_CLOSE;
    }
    fail: {
        return GPU_RESULT_FAIL;
    }
}

GpuResult gpu_cmd_screen_end(GpuContext* context) {
    GpuVulkanDevice*        vulkan_device        = &context->vulkan_device;
    GpuPersistentResources* persistent_resources = &context->persistent_resources;
    GpuCmd*                 cmd                  = &context->cmd;

    VkDevice        device                  = vulkan_device->device;
    VkCommandBuffer command_buffer          = vulkan_device->command_buffer_render;
    VkFence         render_fence            = context->sync_objects.render_fence;
    VkSemaphore     image_acquire_semaphore = context->sync_objects.image_acquire_semaphore;
    VkSemaphore*    image_submit_semaphores = context->sync_objects.image_submit_semaphores;

    u32     render_image_id = cmd->render_image_id;
    VkImage render_image    = cmd->render_image;

    /* bottom render barrier */ {
        VkImageMemoryBarrier render_image_barrier = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask    = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            .dstAccessMask    = VK_ACCESS_NONE,
            .oldLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout        = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image            = render_image,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .subresourceRange = (VkImageSubresourceRange) {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        };
        vkCmdPipelineBarrier(
            command_buffer, 
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0,
            NULL,
            0,
            NULL,
            1,
            &render_image_barrier
        );
    }

    /* submit & present */ {
        if(vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            LOG_ERROR("failed to end render command buffer");
            goto fail;
        }

        /* submit */
        const VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        const VkSubmitInfo submit_render_queue_info = {
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &command_buffer,
            .waitSemaphoreCount   = 1,
            .pWaitSemaphores      = &image_acquire_semaphore,
            .pWaitDstStageMask    = &wait_stages,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores    = &image_submit_semaphores[render_image_id]
        };

        if(vkQueueSubmit(vulkan_device->queue_render, 1, &submit_render_queue_info, render_fence) != VK_SUCCESS) {
            LOG_ERROR("failed to submit frame to render queue");
            goto fail;
        }

        /* present */
        const VkPresentInfoKHR present_swapchain_image_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .swapchainCount     = 1,
            .pSwapchains        = &persistent_resources->swapchain,
            .pImageIndices      = &render_image_id,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &image_submit_semaphores[render_image_id]
        };

        VkResult present_result = vkQueuePresentKHR(
            vulkan_device->queue_render, 
            &present_swapchain_image_info
        );
        if(present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
            /* resize swapchain */
            if(vkDeviceWaitIdle(device) != VK_SUCCESS) {
                LOG_ERROR("failed to wait for device");
                goto fail;
            }
            
            GpuResult swapchain_resize_result = gpu_recreate_swapchain(context);
            if(swapchain_resize_result == GPU_RESULT_FAIL) {
                LOG_ERROR("failed to resize swapchain");
                goto fail;
            }
            else if(swapchain_resize_result == GPU_RESULT_CLOSE) {
                goto close;
            }
        }
        else if(present_result != VK_SUCCESS) {
            LOG_ERROR("failed to present frame");
            goto fail;
        }
    }

    *cmd = (GpuCmd){0};

    return GPU_RESULT_SUCCESS;

    close: {
        return GPU_RESULT_CLOSE;
    }
    fail: {
        return GPU_RESULT_FAIL;
    }
}

b32 gpu_cmd_offscreen_begin(GpuContext* context) {
    VkDevice        device         = context->vulkan_device.device;
    VkCommandBuffer command_buffer = context->vulkan_device.command_buffer_render;
    VkFence         render_fence   = context->sync_objects.render_fence;

    /* reset fence */
    if(vkWaitForFences(device, 1, &render_fence, TRUE, U64_MAX) != VK_SUCCESS) {
        LOG_ERROR("failed to wait for render fence");
        goto fail;
    }
    if(vkResetFences(device, 1, &render_fence) != VK_SUCCESS) {
        LOG_ERROR("failed to reset render fence");
        goto fail;
    }

    /* begin command buffer */
    const VkCommandBufferBeginInfo command_buffer_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    if(vkResetCommandBuffer(command_buffer, 0) != VK_SUCCESS) {
        LOG_ERROR("failed to reset command buffer");
        goto fail;
    }
    if(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info) != VK_SUCCESS) {
        LOG_ERROR("failed to begin command buffer");
        goto fail;
    }

    /* bind descriptors */
    VkDescriptorSet  descriptor_set  = context->persistent_resources.descriptor_set;
    VkPipelineLayout pipeline_layout = context->persistent_resources.pipeline_layout; 

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_set, 0, NULL);

    return TRUE;

    fail: {
        return FALSE;
    }
}

b32 gpu_cmd_offscreen_end(GpuContext* context) {
    VkQueue         queue          = context->vulkan_device.queue_render;
    VkCommandBuffer command_buffer = context->vulkan_device.command_buffer_render;
    VkFence         render_fence   = context->sync_objects.render_fence;

    if(vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        LOG_ERROR("failed to end command buffer");
        goto fail;
    }

    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &command_buffer
    };

    if(vkQueueSubmit(queue, 1, &submit_info, render_fence) != VK_SUCCESS) {
        LOG_ERROR("failed to submit render queue");
        goto fail;
    } 

    return TRUE;

    fail: {
        return FALSE;
    }
}


void gpu_cmd_memory_barrier(GpuContext* context, u64 address, u64 size) {
    VkCommandBuffer command_buffer = context->vulkan_device.command_buffer_render;

    VkBuffer malloc_buffer         = context->memory_pools.buffer_malloc;
    u64      malloc_buffer_address = context->memory_pools.malloc_buffer_address;
    u64      malloc_heap_size      = context->memory_pools.size_malloc;

    if( size == 0 ||
        address < malloc_buffer_address || 
        size > malloc_heap_size
    ) {
        LOG_ERROR("invalid memory barrier");
        goto fail;
    }

    const VkAccessFlags access_mask = (
        VK_ACCESS_SHADER_WRITE_BIT   |
        VK_ACCESS_SHADER_READ_BIT    | 
        VK_ACCESS_TRANSFER_WRITE_BIT | 
        VK_ACCESS_TRANSFER_READ_BIT
    );

    VkPipelineStageFlags stage_mask = (
        VK_PIPELINE_STAGE_TRANSFER_BIT        |
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT   |
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
    );

    const VkBufferMemoryBarrier buffer_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = access_mask,
        .dstAccessMask = access_mask,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = malloc_buffer,
        .offset              = address - malloc_buffer_address,
        .size                = size
    };

    vkCmdPipelineBarrier(
        command_buffer,
        stage_mask,
        stage_mask,
        0,
        0,
        NULL,
        1,
        &buffer_barrier,
        0,
        NULL
    );

    fail: {}
}

void gpu_cmd_targets_barrier(GpuContext* context, const GpuImageHandle* color_targets, u32 color_targets_count, GpuImageHandle depth_target, b32 clear_color, b32 clear_depth) {
    if(color_targets_count > GPU_MAX_ATTACHMENTS) {
        LOG_ERROR("too many color attachments");
        goto fail;
    }

    GpuCmd* cmd = &context->cmd;

    VkCommandBuffer command_buffer       = context->vulkan_device.command_buffer_render;
    GpuImage*       gpu_images           = context->images_pool.images;
    u32             gpu_max_images_count = context->images_pool.images_max_count;

    VkRenderingAttachmentInfo color_attachment_infos[GPU_MAX_ATTACHMENTS] = {0};
    VkRenderingAttachmentInfo depth_attachment_info = (VkRenderingAttachmentInfo){0};

    /* color images */ {
        for(u32 i = 0; i != color_targets_count; i++) {
            u32 target_image_id = color_targets[i];

            /* surface image */
            if(target_image_id == GPU_SURFACE_IMAGE_ID) {
                color_attachment_infos[i] = (VkRenderingAttachmentInfo) {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .imageView   = cmd->render_image_view,
                    .loadOp      = clear_color ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
                    .storeOp     = VK_ATTACHMENT_STORE_OP_STORE
                };
            } 
            /* resource image */
            else {
                if(target_image_id >= gpu_max_images_count) {
                    LOG_ERROR("invalid image id");
                    goto fail;
                }
                if(gpu_images[target_image_id].image == NULL) {
                    LOG_ERROR("invalid image");
                    goto fail;
                }

                gpu_transit_image(
                    command_buffer, 
                    &gpu_images[target_image_id], 
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                );

                color_attachment_infos[i] = (VkRenderingAttachmentInfo) {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageLayout = gpu_images[target_image_id].layout,
                    .imageView   = gpu_images[target_image_id].view,
                    .loadOp      = clear_color ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
                    .storeOp     = VK_ATTACHMENT_STORE_OP_STORE
                };
            }
        }
    }

    /* depth image */ {
        if(depth_target != GPU_INVALID_HANDLE) {
            gpu_transit_image(
                command_buffer, 
                &gpu_images[depth_target],
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
            );
            
            depth_attachment_info = (VkRenderingAttachmentInfo) {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageLayout = gpu_images[depth_target].layout,
                .imageView   = gpu_images[depth_target].view,
                .loadOp      = clear_depth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp     = VK_ATTACHMENT_STORE_OP_STORE
            };
        }
    }

    cmd->color_attachments_count = color_targets_count;
    cmd->use_depth_attachment    = (depth_target != GPU_INVALID_HANDLE);
    memcpy(cmd->color_attachments, color_attachment_infos, color_targets_count * sizeof(VkRenderingAttachmentInfo));
    cmd->depth_attachment = depth_attachment_info;

    fail: {}
}

void gpu_cmd_sampled_barrier(GpuContext* context, const GpuImageHandle sampled_image) {
    VkCommandBuffer command_buffer       = context->vulkan_device.command_buffer_render;
    GpuImage*       gpu_images           = context->images_pool.images;
    u32             gpu_max_images_count = context->images_pool.images_max_count;

    if(sampled_image >= gpu_max_images_count) {
        LOG_ERROR("invalid image id");
        goto fail;
    }
    if(gpu_images[sampled_image].image == NULL) {
        LOG_ERROR("invalid image");
        goto fail;
    }

    gpu_transit_image(
        command_buffer, 
        &gpu_images[sampled_image], 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    );

    fail: {}
}

void gpu_cmd_storage_barrier(GpuContext* context, const GpuImageHandle storage_image) {
    VkCommandBuffer command_buffer       = context->vulkan_device.command_buffer_render;
    GpuImage*       gpu_images           = context->images_pool.images;
    u32             gpu_max_images_count = context->images_pool.images_max_count;

    if(storage_image >= gpu_max_images_count) {
        LOG_ERROR("invalid image id");
        goto fail;
    }
    if(gpu_images[storage_image].image == NULL) {
        LOG_ERROR("invalid image");
        goto fail;
    }

    gpu_transit_image(
        command_buffer, 
        &gpu_images[storage_image], 
        VK_IMAGE_LAYOUT_GENERAL, 
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    );

    fail: {}
}


void gpu_cmd_begin_rendering(GpuContext* context, u32 width, u32 height) {
    GpuVulkanDevice* vulkan_device  = &context->vulkan_device;
    GpuCmd*          cmd            = &context->cmd;
    VkCommandBuffer  command_buffer = context->vulkan_device.command_buffer_render;
    

    const VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .colorAttachmentCount = cmd->color_attachments_count,
        .pColorAttachments    = cmd->color_attachments,
        .pDepthAttachment     = cmd->use_depth_attachment ? &cmd->depth_attachment : NULL,
        .renderArea           = {{0, 0}, {width, height}},
        .layerCount           = 1
    };

    const VkViewport viewport = {
        .width    = (f32)width,
        .height   = (f32)height,
        .minDepth = 0.0,
        .maxDepth = 1.0,
        .x        = 0.0,
        .y        = 0.0
    };

    vulkan_device->cmd_begin_rendering_khr(command_buffer, &rendering_info);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &rendering_info.renderArea);
}

void gpu_cmd_end_rendering(GpuContext* context) {
    GpuVulkanDevice* vulkan_device  = &context->vulkan_device;
    VkCommandBuffer  command_buffer = context->vulkan_device.command_buffer_render;
    vulkan_device->cmd_end_rendering_khr(command_buffer);
}


void gpu_cmd_sync_memwrite(GpuContext* context, const void* data, u64 size, u64 address) {
    GpuMemoryPools* memory_pools   = &context->memory_pools;
    VkCommandBuffer command_buffer = context->vulkan_device.command_buffer_render;
    VkDevice        device         = context->vulkan_device.device;

    void*                       trasnfer_map           = memory_pools->map_transfer;
    const VkBuffer              transfer_malloc_buffer = memory_pools->buffer_transfer_malloc;
    const VkMemoryPropertyFlags transfer_heap_flags    = memory_pools->flags_transfer; 
    const u64                   transfer_malloc_offset = memory_pools->offset_transfer_malloc;
    const VkDeviceMemory        transfer_heap_memory   = memory_pools->memory_transfer;

    void*                       malloc_map          = memory_pools->map_malloc;
    const VkBuffer              malloc_buffer       = memory_pools->buffer_malloc;
    const u64                   malloc_heap_size    = memory_pools->size_malloc;
    const u64                   malloc_heap_address = memory_pools->malloc_buffer_address;
    const VkMemoryPropertyFlags malloc_heap_flags   = memory_pools->flags_malloc;
    const VkDeviceMemory        malloc_heap_memory  = memory_pools->memory_malloc;

    if(address + size > malloc_heap_address + malloc_heap_size || address < malloc_heap_address) {
        LOG_ERROR("invalid address or size");
        goto fail;
    }

    u64 offset = address - malloc_heap_address;

    /* direct copy */
    if(malloc_map != NULL) {
        memcpy((u8*)malloc_map + offset, data, size);
        if(!(malloc_heap_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            const VkMappedMemoryRange flush_range = {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = malloc_heap_memory,
                .offset = ALIGN_DOWN(offset, 256),
                .size   = ALIGN(size, 256)
            };
            vkFlushMappedMemoryRanges(device, 1, &flush_range);
        }
    }
    /* delayed copy */
    else {
        memcpy((u8*)trasnfer_map + transfer_malloc_offset + offset, data, size);
        if(!(transfer_heap_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            const VkMappedMemoryRange flush_range = {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = transfer_heap_memory,
                .offset = ALIGN_DOWN(transfer_malloc_offset + offset, 256),
                .size   = ALIGN(size, 256)
            };
            vkFlushMappedMemoryRanges(device, 1, &flush_range);
        }

        const VkBufferCopy buffer_copy = {
            .srcOffset = offset,
            .dstOffset = offset,
            .size      = size
        };
        vkCmdCopyBuffer(command_buffer, transfer_malloc_buffer, malloc_buffer, 1, &buffer_copy);
    }

    fail: {}
}

void gpu_cmd_bind_pipeline(GpuContext* context, GpuPipelineHandle pipeline_id) {
    VkCommandBuffer command_buffer  = context->vulkan_device.command_buffer_render;
    GpuPipeline*    pipelines       = context->pipelines.pipelines;
    u32             pipelines_count = context->pipelines.pipelines_count;

    if(pipeline_id >= pipelines_count) {
        LOG_ERROR("invalid pipeline id");
        goto fail;
    }

    if(pipelines[pipeline_id].pipeline_flags & GPU_PIPELINE_FLAG_COMPUTE) {
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[pipeline_id].pipeline);
    } else {
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[pipeline_id].pipeline);
    }

    fail: {}
}

void gpu_cmd_push_constants(GpuContext* context, const void* data, u32 size) {
    VkCommandBuffer  command_buffer  = context->vulkan_device.command_buffer_render;
    VkPipelineLayout pipeline_layout = context->persistent_resources.pipeline_layout;
    vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_ALL, 0, size, data);
}

void gpu_cmd_draw(GpuContext* context, u32 vertices, u32 instances) {
    VkCommandBuffer command_buffer = context->vulkan_device.command_buffer_render;
    vkCmdDraw(command_buffer, vertices, instances, 0, 0);
}

void gpu_cmd_dispatch(GpuContext* context, u32 groups_x, u32 groups_y, u32 groups_z) {
    VkCommandBuffer command_buffer = context->vulkan_device.command_buffer_render;
    vkCmdDispatch(command_buffer, groups_x, groups_y, groups_z);
}


void* gpu_get_glfw_window(GpuContext* context) {
    return context->vulkan_objects.window;
}
