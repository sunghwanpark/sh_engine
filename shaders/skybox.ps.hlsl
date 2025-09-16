// skybox.ps.hlsl

struct psIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

cbuffer cameraCB : register(b0, space0)
{
    float4x4 view;
    float4x4 proj;
    float4x4 inv_view_proj;
    float3 cam_pos;
};

TextureCube<float4> cube_map : register(t1, space0);
SamplerState linear_sampler : register(s2, space0);

float4 main(psIn i) : SV_Target0
{
    float4 clip = float4(i.uv, 1.0, 1.0);

    float4 world_h = mul(inv_view_proj, clip);
    world_h /= world_h.w;
    float3 dir  = normalize(world_h.xyz - cam_pos);
    float3 hdr = cube_map.SampleLevel(linear_sampler, dir, 0).rgb;
    return float4(hdr, 1.0);
}