// lighing.ps.hlsl

// todo
#define SHADOW_USE_HARD 0
#define SHADOW_MAP_CASCADE_COUNT 4
static const float AMBIENT = 0.1;

struct psIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

struct psOut
{
    float4 scene_color : SV_Target0;
};

cbuffer camera : register(b0, space0)
{
    float4x4 inv_proj;        
    float4x4 inv_view;
    float2 near_far;
    float2 pad;

    float4 cascade_splits;
    float3 light_dir;
    float depth_bias;
    float normal_bias;
    float cascade_blend;
    float2 shadow_mapsize;
};

cbuffer light : register(b1, space0)
{
    float4x4 light_viewproj[SHADOW_MAP_CASCADE_COUNT];
};

Texture2D<float4> albedo : register(t2, space0);
Texture2D<float4> normal : register(t3, space0);
Texture2D<float4> metalic_roughness : register(t4, space0);
Texture2D<float> depth : register(t5, space0);
SamplerState linear_sampler : register(s6, space0);

Texture2DArray<float> shadows : register(t7, space0);
SamplerState shadow_sampler : register(s8, space0);

float3 load_world_normal(float2 uv)
{
    float3 n = normal.Sample(linear_sampler, uv).xyz;
    return normalize(n * 2.f - 1.f);
}

float3 reconstruct_world_pos(float2 uv, float depth01)
{
    float4 ndc = float4(uv * 2.0f - 1.0f, depth01, 1.0f);
    float4 view = mul(inv_proj, ndc);
    view /= view.w;
    float4 world = mul(inv_view, view);
    return world.xyz;
}

float get_view_z(float2 uv, float depth01)
{
    float4 ndc = float4(uv * 2.0f - 1.0f, depth01, 1.0f);
    float4 view = mul(inv_proj, ndc);
    view /= view.w;
    return view.z;
}

uint select_cascade(float view_z_n)
{
    uint idx = 0;
    [unroll]
    for (uint i = 0; i < SHADOW_MAP_CASCADE_COUNT; ++i)
    {
        if (view_z_n < cascade_splits[i])
            return i;
    }
    return idx;
}

float sample_shadow_point(uint ci, float3 world_pos)
{
    float4 lclip = mul(light_viewproj[ci], float4(world_pos, 1.0));
    
    float3 proj = lclip.xyz / lclip.w;
    float2 uv = proj.xy * 0.5f + 0.5f;
    if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1 || lclip.w <= 0)
        return 1.0;

    float receiver = proj.z - depth_bias;
    float dist = shadows.SampleLevel(linear_sampler, float3(uv, ci), 0);

    return (dist < receiver) ? 0.0 : 1.0;
}

float sample_shadow_PCF3x3(uint ci, float3 world_pos)
{
    float4 lclip = mul(light_viewproj[ci], float4(world_pos, 1.0));

    float3 proj = lclip.xyz / lclip.w;
    float2 uv = proj.xy * 0.5f + 0.5f;
    if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1 || lclip.w <= 0) 
        return 1.0;

    float  zref = proj.z;
    float  receiver = zref - depth_bias;

    float2 texel = 1.0 / shadow_mapsize;
    float acc = 0.0; int cnt = 0;
    [unroll] 
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float dist = shadows.Sample(shadow_sampler, float3(uv + float2(x, y) * texel, ci));
            acc += (dist < receiver) ? 0.0 : 1.0;
            ++cnt;
        }
    }
    return acc / cnt;
}

float sample_shadow_withblend(float3 worldpos, float view_z_n, uint ci)
{
#if SHADOW_USE_HARD
    return sample_shadow_point(ci, worldpos);
#else
    float s0 = sample_shadow_PCF3x3(ci, worldpos);

    if (cascade_blend <= 0.0) 
        return s0;

    uint  cj = ci;
    float split_l = (ci == 0) ? -1e9 : cascade_splits[ci - 1];
    float split_r = (ci < SHADOW_MAP_CASCADE_COUNT - 1) ? cascade_splits[ci] : 1e9;

    float w = 1.0;
    if (view_z_n > split_r - cascade_blend && ci < SHADOW_MAP_CASCADE_COUNT - 1)
    {
        cj = ci + 1; w = saturate((split_r - view_z_n) / cascade_blend);
    }
    else if (view_z_n < split_l + cascade_blend && ci > 0)
    {
        cj = ci - 1; w = saturate((view_z_n - split_l) / cascade_blend);
    }
    else 
    {
        return s0;
    }

    float s1 = sample_shadow_PCF3x3(cj, worldpos);
    return lerp(s1, s0, w);
#endif
}

static const float PI = 3.14159265;
float D_GGX(float noh, float a)
{
    float a2 = a * a;
    float d = (noh * noh) * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 1e-6);
}
float V_SmithGGX(float nov, float nol, float a)
{
    float k = (a + 1.0); k = (k * k) * 0.125; // ~(a^2)/2
    float gv = nov / max(nov * (1.0 - k) + k, 1e-6);
    float gl = nol / max(nol * (1.0 - k) + k, 1e-6);
    return gv * gl;
}

float3 F_Schlick(float3 f0, float voh)
{
    float f = pow(1.0 - voh, 5.0);
    return f0 + (1.0 - f0) * f;
}

psOut main(psIn i)
{
    psOut o;
    // gbuffer fetch
    float4 albedo4 = albedo.Sample(linear_sampler, i.uv);
    if (albedo4.a <= 0.0)
    {
        o.scene_color = 0.f;
        return o;
    }

    float3 base_color = albedo4.rgb;
    float3 n = load_world_normal(i.uv);
    float4 rm = metalic_roughness.Sample(linear_sampler, i.uv);
    float roughness = saturate(rm.r);
    float metalic = saturate(rm.g);

    float depth01 = depth.Sample(shadow_sampler, i.uv);
    if (depth01 >= 1.0)
    {
        o.scene_color = 0.f;
        return o;
    }

    // world position
    float3 pos_w = reconstruct_world_pos(i.uv, depth01);
    pos_w += n * normal_bias;

    // select cascade
    float view_z = get_view_z(i.uv, depth01);
    //float dist = abs(view_z);
    float view_z_n = saturate((-view_z - near_far.x) / (near_far.y - near_far.x));
    uint ci = select_cascade(view_z_n);
    float shadow = sample_shadow_withblend(pos_w, view_z_n, ci);

    // lighing vectors
    float3 vpos_w = (float3)inv_view[3].xyz;
    float3 v = normalize(vpos_w - pos_w);
    float3 l = normalize(-light_dir);
    float3 h = normalize(v + l);

    // brdf terms
    float nov = saturate(dot(n, v));
    float nol = saturate(dot(n, l));
    float noh = saturate(dot(n, h));
    float voh = saturate(dot(v, h));

    float a = max(roughness * roughness, 0.001f);
    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), base_color, metalic);

    float d = D_GGX(noh, a);
    float vis = V_SmithGGX(nov, nol, a);
    float3 f = F_Schlick(f0, voh);

    float3 specular = (d * vis) * f;
    float3 fresnel_coeff = (1.0f - f) * (1.0f - metalic);
    float3 diffuse = fresnel_coeff * base_color / PI;

    float3 direct = (diffuse + specular) * nol * shadow;
    float3 ambient = AMBIENT * base_color;

    float3 color = ambient + direct;
    o.scene_color = float4(max(color, 0.f), 1.f);
    return o;
}