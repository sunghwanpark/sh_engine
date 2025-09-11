// gbuffer.vs.hlsl
#include "common.hlsli"

cbuffer globals : register(b0, space0)
{
    float4x4 view_proj;
};

StructuredBuffer<instanceData> instances : register(t0, space1);

struct vsIn 
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct vsOut 
{
    float4 pos : SV_Position;
    float3 normal : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

vsOut main(vsIn i, uint instId : SV_InstanceID)
{
    vsOut o;
    float4 wp = mul(instances[instId].model, float4(i.pos, 1.0));
    o.pos = mul(view_proj, wp);
    o.normal = mul(instances[instId].normal_mat, i.normal);
    o.uv = i.uv;
    return o;
}
