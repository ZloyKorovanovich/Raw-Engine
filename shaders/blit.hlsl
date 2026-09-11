#include "descriptors.hlsl"

struct PushConstants {
    uint src_image_id;
    uint sampler_id;
};

struct Interpolators {
    float4 position_cs : SV_Position;
    float4 screen_uv   : Texcoord0;
};

[[vk::push_constant]] PushConstants push_constants;

Interpolators vs_main(uint vertex_id : SV_VertexId) {
    Interpolators output = (Interpolators)0;
    output.screen_uv   = float4((vertex_id << 1) & 2, vertex_id & 2, 0.0, 0.0);
    output.position_cs = float4(output.screen_uv.xy * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

float4 fs_main(Interpolators input) : SV_Target0 {
    uint image_id   = push_constants.src_image_id;
    uint sampler_id = push_constants.sampler_id;
    return sampled_images[image_id].SampleLevel(samplers[sampler_id], input.screen_uv.xy, 0).rgba;
}