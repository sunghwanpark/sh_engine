// equirect_to_cube_cs.hlsl

Texture2D<float4> equirect : register(t0, space0);
SamplerState linear_sampler : register(s1, space0);

struct convertCB
{
    uint dst_size;
    uint mip_level;
};
[[vk::push_constant]] convertCB cb;
RWTexture2DArray<float4> out_cube[] : register(u0, space1);

float3 face_dir(uint face, float2 uv) // uv: [-1,1]
{
    float x = uv.x;
    float y = uv.y;
    [branch]
    if (face == 0) return normalize(float3(1, -y, -x)); // +X
    else if (face == 1) return normalize(float3(-1, -y, x)); // -X
    else if (face == 2) return normalize(float3(x, 1, y)); // +Y
    else if (face == 3) return normalize(float3(x, -1, -y)); // -Y
    else if (face == 4) return normalize(float3(x, -y, 1)); // +Z
    else return normalize(float3(-x, -y, -1));    // -Z
}

float2 dir_to_latlongUV(float3 d)
{
    d = normalize(d);
    float u = atan2(d.z, d.x) * (0.5 / 3.1415926535) + 0.5;
    float v = acos(clamp(d.y, -1.0, 1.0)) / 3.1415926535;
    return float2(u, v);
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID,
    uint3 gtid : SV_GroupThreadID,
    uint3 gid : SV_GroupID)
{
    uint face = gid.z;
    uint idx = NonUniformResourceIndex(cb.mip_level);

    if (tid.x >= cb.dst_size || tid.y >= cb.dst_size)
        return;

    float2 uv01 = (float2(tid.xy) + 0.5) / float(cb.dst_size);
    float2 uv11 = uv01 * 2.0 - 1.0;

    float3 dir = face_dir(face, uv11);
    float2 uv = dir_to_latlongUV(dir);

    float3 hdr = equirect.SampleLevel(linear_sampler, uv, 0).rgb;

    out_cube[idx][int3(tid.xy, face)] = float4(hdr, 1.0);
}
