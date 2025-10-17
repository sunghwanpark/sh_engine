// translucent.vs.hlsl
#include "common.hlsli"

cbuffer globals : register(b0, space0)
{
    float4x4 view;
    float4x4 proj;
    float4x4 view_proj;
    float3 camera_pos;
};

StructuredBuffer<instanceData> instances : register(t0, space1);

struct vsIn 
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 tangent : TANGENT;   // .w = handedness(+1/-1)
};

struct vsOut 
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float3 t : TEXCOORD1; 
    float3 b : TEXCOORD2;
    float3 n : TEXCOORD3;
    float3 pos_w : TEXCOORD4;
    float3 view_w : TEXCOORD5;
    float depth_linear : TEXCOORD6;
};

struct vsBuiltins
{
    [[vk::builtin("BaseInstance")]] uint baseInstance : TEXCOORD4;
};

vsOut main(vsIn i, uint instId : SV_InstanceID, vsBuiltins sys)
{
    uint instance_id = sys.baseInstance + instId;

    vsOut o;

    // World/Clip
    float4 wp = mul(instances[instance_id].model, float4(i.pos, 1.0));
    float4 vp = mul(view, wp);
    o.pos = mul(proj, vp);

    // World-space TBN
    float3x3 N = (float3x3)instances[instance_id].normal_mat;
    float3 n_w = normalize(mul(N, i.normal));
    float3 t_w = normalize(mul(N, i.tangent.xyz));
    t_w = normalize(t_w - n_w * dot(n_w, t_w));

    // Determine bitangent sign (handle negative scaling)
    float3x3 model3x3 = (float3x3)instances[instance_id].model;
    float handed_model = (determinant(model3x3) < 0.0f) ? -1.0f : 1.0f;
    float3 b_w = normalize(cross(n_w, t_w) * (i.tangent.w * handed_model));

    o.n = n_w;
    o.t = t_w;
    o.b = b_w;

    // Extra for translucent shading
    o.pos_w = wp.xyz;
    o.view_w = camera_pos - wp.xyz;
    o.depth_linear = -vp.z;
    o.uv = i.uv;

    return o;
}
