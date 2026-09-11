#include "descriptors.hlsl"

struct PushConstants {
    uint64_t uniform_address;
};

struct Interpolators {
    float4 position_cs : SV_Position;
    float4 position_ws : Texcoord0;
};

[[vk::push_constant]] PushConstants push_constants;

static float2 quad_positions[] = {
    float2(0.0, 0.0),
    float2(0.0, 1.0),
    float2(1.0, 1.0),
    float2(0.0, 0.0),
    float2(1.0, 1.0),
    float2(1.0, 0.0)
};

Interpolators vs_main(uint vertex_id : SV_VertexId) {
    uint   vert_id  = vertex_id % 6;
    uint   quad_id  = vertex_id / 6;
    float2 quad_pos = quad_positions[vert_id] * 2.0 - 1.0;

    /* fuck it looks so ugly */
    vk::BufferPointer<UniformBuffer> uniform_buffer = vk::BufferPointer<UniformBuffer>(push_constants.uniform_address);

    Interpolators output = (Interpolators)0;
    output.position_ws = (
        (quad_id == 0) * float4(quad_pos.x, quad_pos.y, 0, 1) +
        (quad_id == 1) * float4(quad_pos.x, 0, quad_pos.y, 1) +
        (quad_id == 2) * float4(0, quad_pos.x, quad_pos.y, 1)
    );
    output.position_cs = mul(uniform_buffer.Get().camera_vp, output.position_ws);
    return output;
}

float4 fs_main(Interpolators input) : SV_Target0 {
    return (input.position_ws + 1.0) * 0.5;
}
