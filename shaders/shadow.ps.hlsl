// shadow_ps.hlsl
#include "common.hlsli"

#if defined(SHADOW_WRITE_OPACITY)
Texture2D     textures[] : register(t1, space1);
SamplerState  samplers[] : register(s0, space1);

struct shadowMaterial
{
    [[vk::offset(72)]]  uint base_color_index;
    uint base_sampler_index;
    float opacity_scale;   // 1.0 means use alpha as-is
    float alpha_cutoff;    // matches your gbuffer usage
};
[[vk::push_constant]] shadowMaterial material;

struct psIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

struct psOut
{
    float depth : SV_Target0;
};

psOut main(psIn i)
{
    psOut o;

    uint texIdx  = NonUniformResourceIndex(material.base_color_index);
    uint sampIdx = NonUniformResourceIndex(material.base_sampler_index);

    float alpha = textures[texIdx].SampleLevel(samplers[sampIdx], i.uv, 0.0f).a;

    if (material.alpha_cutoff > 0.0f && alpha < material.alpha_cutoff)
        discard;

    float opacity = saturate(alpha * material.opacity_scale);
    float transmittance = 1.0f - opacity;
    o.depth = transmittance;
    return o;
}

#else
void main(float4 pos : SV_Position)
{
}
#endif