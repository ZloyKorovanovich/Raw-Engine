#include "descriptors.hlsl"

struct PushConstants {
    float4 color;
};

[[vk::push_constant]] PushConstants push_constants;

float2 full_screen_quad(uint vertex_id) {
    return float2((vertex_id << 1) & 2, vertex_id & 2);
}

float4 vs_main(uint vertex_id : SV_VertexId) : SV_Position {
    return float4(full_screen_quad(vertex_id) * 2.0 - 1.0, 0.0, 1.0);
}

float4 fs_main(float4 position_cs : SV_Position) : SV_Target0 {
    return push_constants.color;
}

[numthreads(4, 4, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadId) {
    storage_images_float[1][thread_id.xy] = float4(1, 1, 1, 0);
}
