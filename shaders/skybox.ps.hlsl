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
};

TextureCube<float4> cube_map : register(t1, space0);
SamplerState linear_sampler : register(s2, space0);

float4 main(psIn i) : SV_Target0
{
    float2 ndc = float2(i.uv.x * 2.0 - 1.0, 1.0 - i.uv.y * 2.0);
    float3 view_dir = normalize(float3(ndc, 1.0));
    float3x3 inv_view_rot = transpose((float3x3)view);
    float3 dir  = normalize(mul(inv_view_rot, view_dir));
    float3 hdr = cube_map.SampleLevel(linear_sampler, dir, 0).rgb;
    return float4(hdr, 1.0);
}