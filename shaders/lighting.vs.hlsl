// lighting.vs.hlsl
#include "common.hlsli"

struct vsIn 
{ 
    float3 pos : POSITION; 
};

struct vsOut 
{ 
    float4 pos : SV_Position; 
};

vsOut main(vsIn i, uint instId : SV_InstanceID)
{
    vsOut o;
    return o;
}