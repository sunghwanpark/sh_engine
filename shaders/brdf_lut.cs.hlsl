// brdf_lut.cs.hlsl

#include "constant.hlsli"

// Generate BRDF integration LUT for GGX + Smith + Schlick Fresnel
// x = NoV in [0,1], y = Roughness in [0,1], output RG = (A,B)

RWTexture2D<float2> brdf_lut : register(u0, space0);

struct brdfPC
{
    uint num_samples;
};
[[vk::push_constant]] brdfPC pc;

// Smith GGX Geometry term (separable Schlick-GGX form)
float g_schlick_GGX(float n_dot_x, float k)
{
    return n_dot_x / max(n_dot_x * (1.0 - k) + k, 1e-6);
}
float g_smith_schlick_GGX(float n_dot_v, float n_dot_l, float roughness)
{
    // common choice: k = (a+1)^2 / 8  with a=roughness
    float a = roughness;
    float k = (a + 1.0);
    k = (k * k) * 0.125;
    float gv = g_schlick_GGX(n_dot_v, k);
    float gl = g_schlick_GGX(n_dot_l, k);
    return gv * gl;
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint width, height;
    brdf_lut.GetDimensions(width, height);

    uint x = tid.x, y = tid.y;
    if (x >= width || y >= height) 
        return;

    float nov = ((float)x + 0.5) / (float)width;
    float roughness = ((float)y + 0.5) / (float)height;

    float3 n = float3(0, 0, 1);
    float  sin_theta_v = sqrt(saturate(1.0 - nov * nov));
    float3 v = float3(sin_theta_v, 0.0, nov);

    float a = max(roughness, 1e-4);
    float a2 = a * a;

    float r = 0.0;
    float g = 0.0;

    [loop]
    for (uint i = 0; i < pc.num_samples; ++i)
    {
        float2 xi = hammersley(i, pc.num_samples);
        float3 h = importance_sample_GGX_H(xi, a, n);
        float3 l = normalize(reflect(-v, h));

        float n_dot_l = saturate(l.z);
        float n_dot_v = nov;
        if (n_dot_l <= 0.0) 
            continue;

        float n_dot_h = saturate(h.z);
        float v_dot_h = saturate(dot(v, h));
        float voh = v_dot_h;

        // Geometry term 
        float geo_term = g_smith_schlick_GGX(n_dot_v, n_dot_l, roughness);

        // Fresnel Schlick
        float fc = pow(1.0 - voh, 5.0);

        // Vis term = geo_term * v_dot_h / (n_dot_h * n_dot_v)
        float vis = (geo_term * v_dot_h) / max(n_dot_h * n_dot_v, 1e-6);

        // integral 
        // E[ (1-Fc) * Vis ] → A term
        // E[ Fc     * Vis ] → B term
        r += (1.0 - fc) * vis;
        g += fc * vis;
    }

    r /= pc.num_samples;
    g /= pc.num_samples;

    brdf_lut[uint2(x, y)] = float2(r, g);
}
