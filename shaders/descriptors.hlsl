#define MAX_IMAGES_COUNT (1024)
#define SAMPLERS_COUNT   (4)

/* non-aliased descriptors */

[[vk::binding(0, 0)]] SamplerState samplers      [SAMPLERS_COUNT  ];
[[vk::binding(1, 0)]] Texture2D    sampled_images[MAX_IMAGES_COUNT];
[[vk::binding(1, 0)]] Texture2DMS<float4> sampled_images_float_ms[MAX_IMAGES_COUNT];

/* alised descriptor arrays for storage images */

[[vk::binding(2, 0)]] [[vk::image_format("unknown")]] RWTexture2D<uint4>  storage_images_uint [MAX_IMAGES_COUNT];
[[vk::binding(2, 0)]] [[vk::image_format("unknown")]] RWTexture2D<int4>   storage_images_int  [MAX_IMAGES_COUNT];
[[vk::binding(2, 0)]] [[vk::image_format("unknown")]] RWTexture2D<float4> storage_images_float[MAX_IMAGES_COUNT];

/* custom structures (located via address in push constants) */

struct UniformBuffer {
    float4x4 camera_vp;
    float4x4 camera_iv;
    float4   sun_dir;
    float4   sun_color;
};

struct Vertex {
    float4 position;
    float4 normal;
};
