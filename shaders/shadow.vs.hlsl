// shadow.vs.hlsl
#include "common.hlsli"

struct pushContant
{
    float4x4 light_view_proj;
    uint cascade_index;
};

[[vk::push_constant]] pushContant pc;
StructuredBuffer<instanceData> instances : register(t0, space0);

struct vsIn 
{ 
    float3 pos : POSITION; 
};

struct vsOut 
{ 
    float4 pos : SV_Position; 
    uint layer_idx : SV_RenderTargetArrayIndex;
};

vsOut main(vsIn i, uint instId : SV_InstanceID)
{
    vsOut o;
    float4 wp = mul(instances[instId].model, float4(i.pos, 1.0));
    o.pos = mul(pc.light_view_proj, wp);
    o.layer_idx = pc.cascade_index;
    return o;
}