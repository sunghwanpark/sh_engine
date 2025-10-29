#pragma once

#include "pch.h"
#include "rhi/rhiResource.h"

class rhiDeviceContext;
class rhiBuffer;
class rhiTexture;
class rhiSampler;
class glTFMesh;
class renderShared;
class textureCache;
class rhiRenderResource : public rhiResource
{
public:
    rhiRenderResource(std::weak_ptr<glTFMesh> raw_mesh);

public:
    struct meshletVerticesSet
    {
        vec3 position;
        vec3 normal;
        vec2 uv;
        vec4 tangent;
    };

    struct meshletRange
    {
        u32 first_meshlet;
        u32 meshlet_count;
        u32 header_offset;
        u32 vert_offset;
        u32 tri_byte_offset;
        u32 bound_offset;
    };

    struct subMesh 
    {
        u32 first_index = 0;
        u32 index_count = 0;
        mat4 model = mat4(1.f);
        u32 material_slot = 0;
        
        // meshlet gltf ref
        std::vector<meshopt_Meshlet>* meshlets;
        std::vector<meshopt_Bounds>* meshlet_bounds;
        std::vector<u32>* meshlet_vertices;
        std::vector<u8>* meshlet_triangles;
        u32 first_meshlet;
        u32 meshlet_count;
    };

    struct material 
    {
        std::shared_ptr<rhiTexture> base_color;
        std::shared_ptr<rhiTexture> norm_color;
        std::shared_ptr<rhiTexture> m_r_color;
        
        std::shared_ptr<rhiSampler> base_sampler;
        std::shared_ptr<rhiSampler> norm_sampler;
        std::shared_ptr<rhiSampler> m_r_sampler;

        f32 alpha_cutoff;
        f32 metalic_factor = 0.f;
        f32 roughness_factor = 1.f;

        bool is_translucent;
        bool is_double_sided;
    };

public:
    void upload(renderShared* rs, textureCache* tex_cache);
    void make_meshlet_resource(textureCache* tex_cache);
    rhiBuffer* get_vbo() const;
    rhiBuffer* get_ibo() const;
    const material& get_material(const i32 slot_index);
    glTFMesh* get_raw_data() { auto ptr = raw_data.lock();  return ptr.get(); }
    const std::vector<subMesh>& get_submeshes() const { return submeshes; }
    std::vector<subMesh>& get_submeshes_mutable() { return submeshes; }

private:
    void rebuild_submeshes(textureCache* tex_cache, glTFMesh* raw_mesh);
    bool is_uploaded() const { return vbo != nullptr && ibo != nullptr; }

private:
    std::weak_ptr<glTFMesh> raw_data;
    std::unique_ptr<rhiBuffer> vbo;
    std::unique_ptr<rhiBuffer> ibo;
    std::vector<subMesh> submeshes;
    std::vector<material> materials;
};