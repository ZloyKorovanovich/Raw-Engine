#include "descriptors.hlsl"

struct PushConstants {
    uint src_image_id;
    uint ms_count;
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
    Texture2DMS<float4> src = sampled_images_float_ms[push_constants.src_image_id];

    int2 pixel = int2(input.position_cs.xy);

    float4 accum = 0.0;
    for (uint s = 0; s < push_constants.ms_count; ++s) {
        accum += src.Load(pixel, s);
    }

    return accum / float(push_constants.ms_count);
}