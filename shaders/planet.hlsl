#include "descriptors.hlsl"

struct PushConstants {
    uint64_t uniform_address;
    uint     resolution;
    uint     none_0;
};

struct Interpolators {
    float4 position_cs : SV_Position;
    float4 position_ws : Texcoord0;
    float4 normal_ws   : Texcoord1;
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

float3 pyramid(uint vertex_id, uint resolution) {
    uint vert_id = vertex_id % 6;
    uint quad_id = vertex_id / 6;

    float2 lpos = quad_positions[vert_id];
    float2 gpos = float2(quad_id % (resolution - 1), quad_id / (resolution - 1));
    float2 pos  = (lpos + gpos) * 2.0 / (resolution - 1) - 1.0;
    
    float dist = 1.0 - max(abs(pos.x), abs(pos.y));

    return float3(pos, dist);
}

float4x4 look_towards(float3 direction, float3 up) {
    float3 forward = normalize(direction);
    float3 right   = normalize(cross(up, forward));
    float3 true_up = cross(forward, right);

    return float4x4(
        right.x,   true_up.x,   forward.x,   0.0,
        right.y,   true_up.y,   forward.y,   0.0,
        right.z,   true_up.z,   forward.z,   0.0,
        0.0,       0.0,         0.0,         1.0
    );
}

Interpolators vs_main(uint vertex_id : SV_VertexId) {
    vk::BufferPointer<UniformBuffer> uniform_buffer = vk::BufferPointer<UniformBuffer>(push_constants.uniform_address);

    float4x4 camera_iv  = uniform_buffer.Get().camera_iv;
    float3   camera_pos = float3(camera_iv[0][3], camera_iv[1][3], camera_iv[2][3]);

    float4x4 look_at_camera = look_towards(camera_pos, float3(0.0, 0.0, 1.0));
    float3 pyramid_pos = pyramid(vertex_id, push_constants.resolution);
    float3 sphere_pos  = normalize(mul(look_at_camera, float4(pyramid_pos, 1.0)).xyz);

    Interpolators output = (Interpolators)0;
    output.position_ws = float4(sphere_pos, 1.0);
    output.normal_ws   = float4(sphere_pos, 0.0);
    output.position_cs = mul(uniform_buffer.Get().camera_vp, output.position_ws);
    return output;
}

float4 fs_main(Interpolators input) : SV_Target0 {
    vk::BufferPointer<UniformBuffer> uniform_buffer = vk::BufferPointer<UniformBuffer>(push_constants.uniform_address);


    float3 sun_dir = uniform_buffer.Get().sun_dir.xyz;
    float3 normal  = normalize(input.normal_ws.xyz);

    return float4(1.0, 0.0, 1.0, 1.0) * (dot(sun_dir, normal) * 0.5 + 0.5);
}
