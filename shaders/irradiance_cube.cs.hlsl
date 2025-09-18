// irradiance_cubs.cs.hlsl

#include "constant.hlsli"

TextureCube<float4> sky_cube : register(t0, space0);
SamplerState sky_sampler : register(s1, space0);

RWTexture2DArray<float4> irradiance_cube : register(u2, space0);

cbuffer params : register(b3, space0)
{
    uint out_size;        // out cube face resolution (32) 
    uint num_samples;     // irradiance samples (256~1024)
};

float3 sample_hemisphere_cosine(float2 xi)
{
    // Cosine-weighted hemisphere
    float r = sqrt(xi.x);
    float phi = 2.0 * PI * xi.y;
    float x = r * cos(phi);
    float y = r * sin(phi);
    float z = sqrt(max(0.0, 1.0 - xi.x));
    return float3(x, y, z);
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID, 
    uint3 gid : SV_GroupID)
{
    // Dispatch: (ceil(OutSize/8), ceil(OutSize/8), 6)
    uint x = tid.x;
    uint y = tid.y;
    uint face = gid.z; // 0..5

    if (x >= out_size || y >= out_size || face >= 6) return;

    float2 uv = (float2(x + 0.5, y + 0.5) / out_size) * 2.0 - 1.0;
    float3 n = face_dir(face, uv);
    float3 t;
    float3 b;
    build_tangent_frame(n, t, b);

    float3 sum = 0.0.xxx;
    [loop]
    for (uint i = 0; i < num_samples; ++i)
    {
        float2 xi = hammersley(i, num_samples);
        float3 h = sample_hemisphere_cosine(xi);     // local hemisphere
        float3 l = normalize(t * h.x + b * h.y + n * h.z); // world dir

        float3 c = sky_cube.SampleLevel(sky_sampler, l, 0).rgb;
        sum += c;
    }
    // e(n) ≈ (π / n) Σ L(ω_i)  (cosθ/pdf == π for cosine-weighted)
    float3 e = (PI / max(1u, num_samples)) * sum;
    irradiance_cube[uint3(x, y, face)] = float4(e, 1.0);
}
