// translucent.ps.hlsl
#define SHADOW_MAP_CASCADE_COUNT 4

struct psIn 
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

struct psOut 
{
#if DISABLE_OIT
    float4 color : SV_Target0;
#else
    float4 accum : SV_Target0;
    float4  reveal : SV_Target1;
#endif
};

cbuffer globals : register(b0, space0)
{
    float4x4 view;
    float4x4 proj;
    float4x4 view_proj;
    float3 camera_pos;
};

cbuffer lights : register(b0, space2)
{
    float4x4 light_viewproj[SHADOW_MAP_CASCADE_COUNT];
    float3 light_dir;
    float4 cascade_splits;
    float2 shadow_mapsize;
};
Texture2DArray<float> shadows : register(t1, space2);
SamplerState shadow_sampler : register(s2, space2);

SamplerState samplers[] : register(s0, space3);
Texture2D textures[] : register(t1, space3);

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

// --------------------- Helpers / BRDF -------------------------
static const float PI = 3.14159265;
float3 F_Schlick(float3 f0, float voh)
{
    float f = pow(1.0 - voh, 5.0);
    return f0 + (1.0 - f0) * f;
}

float D_GGX(float noh, float a)
{
    float a2 = a * a;
    float d = (noh * noh) * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 1e-6);
}

float  V_SmithGGXCorrelated(float nov, float nol, float a) 
{
    float a2 = a * a;
    float gv = nol * sqrt(nov * (nov - nov * a2) + a2);
    float gl = nov * sqrt(nol * (nol - nol * a2) + a2);
    return 0.5 / max(gv + gl, 1e-6);
}

float select_cascade(float view_z)
{
    float dist = abs(view_z);
    uint idx = 0;
    [unroll]
    for (uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1u; ++i)
    {
        idx += (dist > cascade_splits[i]) ? 1u : 0u;
    }
    return (float)idx;
}

float sample_shadow_PCF3x3(float3 world_pos)
{
    float4 pos_v = mul(view, float4(world_pos, 1.0));
    uint ci = (uint)select_cascade(pos_v.z);
    ci = min(ci, SHADOW_MAP_CASCADE_COUNT - 1u);

    float4 lclip = mul(light_viewproj[ci], float4(world_pos, 1.0));
    if (lclip.w <= 0.0) 
        return 1.0;

    float3 proj = lclip.xyz / lclip.w;
    float2 uv = proj.xy * 0.5f + 0.5f;
    float  zref = proj.z;

    if (any(uv < 0.0) || any(uv > 1.0)) 
        return 1.0;

    float depthBias = 0.001f;
    float receiver = zref - depthBias;

    float2 texel = 1.0 / shadow_mapsize;
    const int radius = 1; // 3x3
    float acc = 0.0;

    [unroll]
    for (int y = -radius; y <= radius; ++y)
    {
        [unroll]
        for (int x = -radius; x <= radius; ++x)
        {
            float2 ofs = float2(x, y) * texel;
            float stored = shadows.SampleLevel(shadow_sampler, float3(uv + ofs, ci), 0).r;
            acc += (stored < receiver) ? 0.0 : 1.0;
        }
    }
    return acc / ((2 * radius + 1) * (2 * radius + 1));
}

psOut main(psIn i)
{
    psOut o;

    // albedo
    uint b_idx = NonUniformResourceIndex(materials.base_color_index);
    uint b_s_idx = NonUniformResourceIndex(materials.base_sampler_index);
    float4 albedo = textures[b_idx].Sample(samplers[b_s_idx], i.uv);
    float alpha = saturate(albedo.a);
    if (albedo.a <= 1e-4)
    {
#if DISABLE_OIT
        o.color = 0;
#else
        o.accum = 0; 
        o.reveal = 0; 
#endif
        return o;
    }

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

    // ---- Shading vecs ----
    float3 v = normalize(i.view_w);
    float3 l = normalize(-light_dir);
    float3 h = normalize(v + l);
    n_w = faceforward(n, -v, n);

    float nol_f = saturate(dot(n_w, l));
    float nol_b = saturate(dot(-n_w, l));

    float nov = saturate(dot(n_w, v));
    float noh = saturate(dot(n_w, h));
    float voh = saturate(dot(v, h));

    float3 base_color = albedo.rgb;

    // ---- fresnel & kd ----
    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), base_color, metalic);
    float3 fv = F_Schlick(f0, voh);
    float3 kd = (1.0 - fv) * (1.0 - metalic);

    // ---- shadows ----
    float shadow = sample_shadow_PCF3x3(i.pos_w);
    shadow = lerp(1.0, shadow, saturate(alpha));
    const float shadow_floor = 0.5;
    shadow = max(shadow, shadow_floor);

    // --- BRDF ---
    float  a = max(1e-3, roughness * roughness);
    float  d = D_GGX(noh, a);
    float  vis = V_SmithGGXCorrelated(nov, nol_f, a);
    float3 spec = d * vis * fv * nol_f;
    float3 diffuse = kd * base_color * (nol_f / PI);

    // ---- simple back light ----
    const float sss_strength = 0.5;
    float3 sss_color = base_color; 
    diffuse += sss_color * nol_b * sss_strength;

    // ---- ambient ----
    const float3 world_up = float3(0, 1, 0);
    float hemi = 0.5f * dot(n, world_up) + 0.5f;

    const float3 ambient_top = float3(0.04, 0.05, 0.06);
    const float3 ambient_bottom = float3(0.02, 0.02, 0.02);

    float3 ambient_lerp = lerp(ambient_bottom, ambient_top, hemi);
    float3 ambient = ambient_lerp * kd * base_color;

    // ---- Lighting (single directional) ----
    float3 lit = ambient + (diffuse + spec);

#if DISABLE_OIT
    o.color = float4(lit * alpha, alpha);
#else
    // ---- WOIT premultiplied output ----
    float w = saturate(1e-3 + 3e3 * exp(-8.0 * i.depth_linear * i.depth_linear));
    float3 premul = lit * alpha * w;
    o.accum = float4(premul, alpha * w);
    o.reveal.a = alpha;
#endif

    return o;
}
