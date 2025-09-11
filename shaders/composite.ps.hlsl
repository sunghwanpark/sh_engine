// composite.ps.hlsl
cbuffer compositeCB : register(b0, space0)
{
    float2 invRT;
    float  exposure;
    float  pad;
};

Texture2D scene_color : register(t1, space0);
SamplerState s0 : register(s2, space0);

struct psIn 
{ 
    float4 pos : SV_Position; 
    float2 uv : TEXCOORD0; 
};

float3 tonemapACES(float3 x)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 main(psIn i) : SV_Target
{
    float3 s = scene_color.Sample(s0, i.uv).rgb;
    // todo
    // float3 nrm = normalize(g1.Sample(s0, i.uv).xyz * 2 - 1);
    // float3 gbufferc = g2.Sample(s0, i.uv);
    // float roughness = gbufferc.r
    // float metalic = gbuffer.g

    float3 color = s * exposure;
    //color = tonemapACES(color);
    //color = pow(color, 1.0 / 2.2);

    return float4(color, 1.0);
}
