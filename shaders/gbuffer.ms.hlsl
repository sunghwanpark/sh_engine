// gbuffer.ms.hlsl
#include "common.hlsli"
#include "common_meshlet.hlsli"
#include "meshlet_pc.hlsli"

cbuffer globals : register(b0, space0)
{
    float4x4 view;
    float4x4 proj;
};

StructuredBuffer<instanceData> instances : register(t0, space1);
StructuredBuffer<meshletDrawParams> draw_params : register(t1, space1);

StructuredBuffer<float4> position : register(t2, space1);
StructuredBuffer<uint> normal : register(t3, space1); // 10:10:10:2
StructuredBuffer<uint> uv : register(t4, space1); // half2 packed
StructuredBuffer<uint> tangent : register(t5, space1); // 10:10:10:2

StructuredBuffer<meshletHeader> meshlets : register(t6, space1);
StructuredBuffer<uint> meshlet_vertex_index : register(t7, space1);
ByteAddressBuffer meshlet_tribyte_list : register(t8, space1);

#define MS_THREADS 64u
#define MS_MAX_PRIMS MS_THREADS
#define MS_MAX_VERTS (MS_MAX_PRIMS * 3u)

struct vsOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float3 t : TEXCOORD1;
    float3 b : TEXCOORD2;
    float3 n : TEXCOORD3;
};

[outputtopology("triangle")]
[numthreads(MS_THREADS, 1, 1)]
void main(
    uint tid : SV_GroupThreadID,
    uint3 gid : SV_GroupID,
    out vertices vsOut out_verts[MS_MAX_VERTS],
    out indices uint3 out_tris[MS_MAX_PRIMS]
)
{
    meshletDrawParams dp = draw_params[pc.dp.dp_index];

    const uint meshlet_index = dp.first_meshlet + gid.x;
    const uint instance_id = dp.first_instance + gid.y;
    
    instanceData inst = instances[instance_id];
    meshletHeader mh = meshlets[meshlet_index];

    const uint tri_count = mh.prim_count;
    const uint vert_count = mh.vertex_count;
    const uint max_tri = min(tri_count, MS_MAX_PRIMS);

    SetMeshOutputCounts(max_tri * 3u, max_tri);
    if (tid >= max_tri)
        return;

    const uint tribyte_base = mh.prim_byte_offset + tid * 3u;
    uint i0_local = load_tri_index(meshlet_tribyte_list, tribyte_base + 0u);
    uint i1_local = load_tri_index(meshlet_tribyte_list, tribyte_base + 1u);
    uint i2_local = load_tri_index(meshlet_tribyte_list, tribyte_base + 2u);

    uint gi0 = meshlet_vertex_index[mh.vertex_offset + i0_local];
    uint gi1 = meshlet_vertex_index[mh.vertex_offset + i1_local];
    uint gi2 = meshlet_vertex_index[mh.vertex_offset + i2_local];

    float3 p0 = position[gi0].xyz;
    float3 p1 = position[gi1].xyz;
    float3 p2 = position[gi2].xyz;

    float3 n0 = unpack1010102_SNORM(normal[gi0]);
    float3 n1 = unpack1010102_SNORM(normal[gi1]);
    float3 n2 = unpack1010102_SNORM(normal[gi2]);

    float3 t0v = unpack1010102_SNORM(tangent[gi0]);
    float3 t1v = unpack1010102_SNORM(tangent[gi1]);
    float3 t2v = unpack1010102_SNORM(tangent[gi2]);

    int h0 = unpack_handed(tangent[gi0]);
    int h1 = unpack_handed(tangent[gi1]);
    int h2 = unpack_handed(tangent[gi2]);

    float2 uv0 = unpack_half2(uv[gi0]);
    float2 uv1 = unpack_half2(uv[gi1]);
    float2 uv2 = unpack_half2(uv[gi2]);

    float4x4 m = inst.model;
    float3x3 n3 = (float3x3)inst.normal_mat;

    float4 wp0 = mul(m, float4(p0, 1.0));
    float4 wp1 = mul(m, float4(p1, 1.0));
    float4 wp2 = mul(m, float4(p2, 1.0));

    float4 vp0 = mul(view, wp0);
    float4 vp1 = mul(view, wp1);
    float4 vp2 = mul(view, wp2);

    float4 hp0 = mul(proj, vp0);
    float4 hp1 = mul(proj, vp1);
    float4 hp2 = mul(proj, vp2);

    float3 nn0 = normalize(mul(n3, n0));
    float3 nn1 = normalize(mul(n3, n1));
    float3 nn2 = normalize(mul(n3, n2));

    float3 tt0 = normalize(mul(n3, t0v));
    float3 tt1 = normalize(mul(n3, t1v));
    float3 tt2 = normalize(mul(n3, t2v));

    tt0 = normalize(tt0 - nn0 * dot(nn0, tt0));
    tt1 = normalize(tt1 - nn1 * dot(nn1, tt1));
    tt2 = normalize(tt2 - nn2 * dot(nn2, tt2));

    // handedness 보정 (모델 행렬 반전 고려)
    float3x3 m3 = (float3x3)m;
    float handedModel = (determinant(m3) < 0.0f) ? -1.0f : 1.0f;

    float3 bb0 = normalize(cross(nn0, tt0) * (float)(h0)*handedModel);
    float3 bb1 = normalize(cross(nn1, tt1) * (float)(h1)*handedModel);
    float3 bb2 = normalize(cross(nn2, tt2) * (float)(h2)*handedModel);

    const uint vtx_base = tid * 3u;

    out_verts[vtx_base + 0].pos = hp0;
    out_verts[vtx_base + 0].uv = uv0;
    out_verts[vtx_base + 0].t = tt0;
    out_verts[vtx_base + 0].b = bb0;
    out_verts[vtx_base + 0].n = nn0;

    out_verts[vtx_base + 1].pos = hp1;
    out_verts[vtx_base + 1].uv = uv1;
    out_verts[vtx_base + 1].t = tt1;
    out_verts[vtx_base + 1].b = bb1;
    out_verts[vtx_base + 1].n = nn1;

    out_verts[vtx_base + 2].pos = hp2;
    out_verts[vtx_base + 2].uv = uv2;
    out_verts[vtx_base + 2].t = tt2;
    out_verts[vtx_base + 2].b = bb2;
    out_verts[vtx_base + 2].n = nn2;

    out_tris[tid] = uint3(vtx_base + 0u, vtx_base + 1u, vtx_base + 2u);
}
