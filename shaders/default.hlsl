#include "descriptors.hlsl"

struct Transform {
    float4x4 matrix_m;
};

struct PushConstants {
    uint64_t uniform_address;
    uint64_t transform_address;
    uint64_t mesh_address;
    uint     vertices_count;
    uint     indices_count;
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

struct Interpolators {
    float4 position_cs : SV_Position;
    float4 normal_ws   : Texcoord0;
};

Interpolators vs_main(uint vertex_id : SV_VertexId) {
    vk::BufferPointer<UniformBuffer> uniform_buffer = vk::BufferPointer<UniformBuffer>(push_constants.uniform_address);
    vk::BufferPointer<Transform>     transform      = vk::BufferPointer<Transform>(push_constants.transform_address);

    vk::BufferPointer<uint>   index  = vk::BufferPointer<uint>(push_constants.mesh_address + push_constants.vertices_count * sizeof(Vertex) + vertex_id * sizeof(uint));
    vk::BufferPointer<Vertex> vertex = vk::BufferPointer<Vertex>(push_constants.mesh_address + index.Get() * sizeof(Vertex));
    
    float3 position_os = vertex.Get().position.xyz;
    float3 position_ws = mul(transform.Get().matrix_m, float4(position_os, 1.0)).xyz;

    float3 normal_os = vertex.Get().normal.xyz;
    float3 normal_ws = normalize(mul((float3x3)transform.Get().matrix_m, normal_os));

    Interpolators output = (Interpolators)0;
    output.position_cs = mul(uniform_buffer.Get().camera_vp, float4(position_ws, 1.0));
    output.normal_ws   = float4(normal_ws, 1.0);

    return output;
}

float4 fs_main(Interpolators input) : SV_Target0 {
    vk::BufferPointer<UniformBuffer> uniform_buffer = vk::BufferPointer<UniformBuffer>(push_constants.uniform_address);
    float3 normal_ws = normalize(input.normal_ws.xyz);
    return float4(0.0, 1.0, 1.0, 1.0) * (1.0 + dot(normal_ws, uniform_buffer.Get().sun_dir.xyz)) * 0.5;
}
