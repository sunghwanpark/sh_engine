#pragma once

#include "pch.h"

namespace meshlet
{
    struct meshletHeader
    {
        u32 vertex_count;
        u32 prim_count;
        u32 vertex_offset;
        u32 prim_byte_offset;
    };

    struct meshletBounds
    {
        vec3 center;
        f32 radius;
        vec3 cone_axis;
        f32 cone_cutoff;
    };

    struct globalMeshlet
    {
        std::vector<meshletHeader> headers;
        std::vector<u32> vertices;
        std::vector<u8> triangles;
        std::vector<meshletBounds> bounds;
    };

    struct meshletDrawParams
    {
        u32 first_meshlet;
        u32 meshlet_count;
        u32 first_instance;
        u32 instance_count;
    }; // 32b

    struct vertexIn
    {
        std::array<f32, 3> pos;
        std::array<f32, 3> nrm;
        std::array<f32, 3> tan;
        std::array<f32, 2> uv;
    };

    struct submeshIn
    {
        std::vector<u32> indices;
    };

    struct meshIn
    {
        std::vector<vertexIn> vertices;
        std::vector<submeshIn> submeshes;
    };

    struct buildOut
    {
        // SoA
        std::vector<vec4> position;  // float4 stride
        std::vector<u32> normal;    // 1010102
        std::vector<u32> tangent;   // 1010102(+handed)
        std::vector<u32> uv;        // half2 packed

        // meshlet
        std::vector<meshletHeader> meshlets;
        std::vector<u32> meshlet_vertex_index; // u16×2 packed
        std::vector<u8> meshlet_tribytes;     // 3B/tri
    };  
}

struct meshletBuffer
{
    std::unique_ptr<rhiBuffer> pos;
    std::unique_ptr<rhiBuffer> norm;
    std::unique_ptr<rhiBuffer> tan;
    std::unique_ptr<rhiBuffer> uv;

    std::unique_ptr<rhiBuffer> header;
    std::unique_ptr<rhiBuffer> vert_indices;
    std::unique_ptr<rhiBuffer> tri_bytes;
};