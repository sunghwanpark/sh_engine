// specular_cube.cs.hlsl

#include "constant.hlsli"

TextureCube<float4> sky_cube : register(t0, space0);
SamplerState sky_sampler : register(s1, space0);
RWTexture2DArray<float4> specular_cube[] : register(u0, space1);

struct PrefilterPC
{
    uint mip_level;
    uint num_samples;
    float roughness;    // [0,1],  m/(mipCount-1)
    float _pad;
};
[[vk::push_constant]] PrefilterPC pc;

// GGX normal distribution
float d_GGX(float n_dot_h, float a)
{
    float a2 = a * a;
    float d = (n_dot_h * n_dot_h) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID, 
    uint3 gid : SV_GroupID)
{
    uint idx = NonUniformResourceIndex(pc.mip_level);
    uint out_w, out_h, out_layers;
    specular_cube[idx].GetDimensions(out_w, out_h, out_layers);

    uint x = tid.x;
    uint y = tid.y;
    uint face = gid.z;
    if (x >= out_w || y >= out_h || face >= 6) 
        return;

    float2 uv = (float2(x + 0.5, y + 0.5) / (float)out_w) * 2.0 - 1.0;
    float3 n = face_dir(face, uv);
    float3 v = n;

    uint sky_w, sky_h, sky_levels;
    sky_cube.GetDimensions(0, sky_w, sky_h, sky_levels);

    // omega_s = 4π / (6 * sky_size^2)
    float omega_s = 4.0 * PI / (6.0 * (float)sky_w * (float)sky_w);
    float mip_max = (float)(sky_levels - 1);

    float a = max(pc.roughness * pc.roughness, 1e-4);

    float3 sum = 0.0.xxx;
    float  sum_weight = 0.0;

    [loop]
    for (uint i = 0; i < pc.num_samples; ++i)
    {
        float2 xi = hammersley(i, pc.num_samples);
        float3 h = importance_sample_GGX_H(xi, a, n);
        float3 l = normalize(reflect(-v, h));

        float n_dot_l = saturate(dot(n, l));
        if (n_dot_l <= 0.0) 
            continue;

        float n_dot_h = saturate(dot(n, h));
        float v_dot_h = saturate(dot(v, h));

        // pdf(L) = D(h) * n_dot_h / (4 * v_dot_h)
        float d = d_GGX(n_dot_h, a);
        float pdf = max(1e-6, (d * n_dot_h) / max(4.0 * v_dot_h, 1e-6));

        float omega_p = 1.0 / ((float)pc.num_samples * pdf);

        float mip = 0.5 * log2(max(omega_p / max(omega_s, 1e-12), 1e-12));
        mip = clamp(mip, 0.0, mip_max);

        float3 c = sky_cube.SampleLevel(sky_sampler, l, mip).rgb;

        float w = n_dot_l / pdf;
        sum += c * w;
        sum_weight += w;
    }

    float3 prefiltered = (sum_weight > 0.0) ? (sum / sum_weight) : 0.0.xxx;
    specular_cube[idx][uint3(x, y, face)] = float4(prefiltered, 1.0);
}
