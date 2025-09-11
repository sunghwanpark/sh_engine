// gbuffer.vs.hlsl
#include "common.hlsli"

cbuffer globals : register(b0, space0)
{
    float4x4 view;
    float4x4 proj;
};

StructuredBuffer<instanceData> instances : register(t0, space1);

struct vsIn 
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 tangent : TANGENT;
};

struct vsOut 
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float3 t : TEXCOORD1;
    float3 b : TEXCOORD2;
    float3 n : TEXCOORD3;
};

vsOut main(vsIn i, uint instId : SV_InstanceID)
{
    vsOut o;
    float4 wp = mul(instances[instId].model, float4(i.pos, 1.0));
    float4 vp = mul(view, wp);
    o.pos = mul(proj, vp);

    float3 n_w = normalize(mul(instances[instId].normal_mat, i.normal));
    float3 t_w = normalize(mul(instances[instId].normal_mat, i.tangent.xyz));
    t_w = normalize(t_w - n_w * dot(n_w, t_w));
    float3 b_w = normalize(cross(n_w, t_w) * i.tangent.w);

    o.n = n_w;
    o.t = t_w;
    o.b = b_w;
    o.uv = i.uv;
    return o;
}
