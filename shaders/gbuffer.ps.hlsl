// gbuffer.ps.hlsl

struct psIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float3 t : TEXCOORD1;
    float3 b : TEXCOORD2;
    float3 n : TEXCOORD3;
};

struct psOut
{
    float4 a : SV_Target0; // albedo
    float4 b : SV_Target1; // normal
    float4 c : SV_Target2; // roughness / metallic
};

SamplerState samplers[] : register(s0, space2);
Texture2D textures[] : register(t1, space2);

struct materialPC
{
    uint base_color_index;
    uint norm_color_index;
    uint mr_color_index;
    uint base_sampler_index;
    uint norm_sampler_index;
    uint mr_sampler_index;

    float alpha_cutoff;
    float metalic_factor;
    float roughness_factor;
}; // 32b
[[vk::push_constant]]
ConstantBuffer<materialPC> materials;

float2 encode_octa(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z) + 1e-8);
    float2 e = n.xy;
    if (n.z < 0.0)
        e = (1.0 - abs(e.yx)) * float2(e.x >= 0 ? 1 : -1, e.y >= 0 ? 1 : -1);
    return e * 0.5 + 0.5;
}

psOut main(psIn i)
{
    psOut o;

    // albedo
    uint b_idx = NonUniformResourceIndex(materials.base_color_index);
    uint b_s_idx = NonUniformResourceIndex(materials.base_sampler_index);
    float4 albedo = textures[b_idx].Sample(samplers[b_s_idx], i.uv);

    if (materials.alpha_cutoff > 0.f && albedo.a < materials.alpha_cutoff)
        discard;

    // normal
    float3 nrm = normalize(i.n);
    if (materials.norm_color_index > 0)
    {
        uint n_idx = NonUniformResourceIndex(materials.norm_color_index);
        uint n_s_idx = NonUniformResourceIndex(materials.norm_sampler_index);
        nrm = textures[n_idx].Sample(samplers[n_s_idx], i.uv).xyz * 2.f - 1.f;
        nrm = normalize(nrm);
    }

    // tbn view
    float3 t = normalize(i.t);
    float3 b = normalize(i.b);
    float3 n = normalize(i.n);
    //float3x3 tbn_w = float3x3(normalize(i.t), normalize(i.b), normalize(i.n));
    float3 n_w = normalize(t * nrm.x + b * nrm.y + n * nrm.z);

    // metalic / roughness
    float roughness = materials.roughness_factor;
    float metalic = materials.metalic_factor;
    if (materials.mr_color_index > 0)
    {
        uint mr_idx = NonUniformResourceIndex(materials.mr_color_index);
        uint mr_s_idx = NonUniformResourceIndex(materials.mr_sampler_index);
        float2 mr = textures[mr_idx].Sample(samplers[mr_s_idx], i.uv).gb;
        roughness = saturate(mr.x * materials.roughness_factor);
        metalic = saturate(mr.y * materials.metalic_factor);
    }

    // write
    o.a = float4(albedo.rgb, albedo.a);
    o.b = float4(n_w * 0.5f + 0.5f, 0.f);
    o.c = float4(roughness, metalic, 0.f, 0.f);

    return o;
}
