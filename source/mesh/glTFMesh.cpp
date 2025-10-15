#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
extern "C" {
#include "mikktspace.h"
}
#include "glTFMesh.h"

struct NodeDraw
{
    const cgltf_primitive* prim;
    glm::mat4 model;
};

static glm::mat4 localMatrix(const cgltf_node* node)
{
    if (node->has_matrix)
    {
        return glm::make_mat4(node->matrix); // column-major
    }
    glm::mat4 T = glm::translate(glm::mat4(1.0f),
        glm::vec3(node->translation[0], node->translation[1], node->translation[2]));

    glm::quat q(node->rotation[3], node->rotation[0], node->rotation[1], node->rotation[2]);
    q = glm::normalize(q);
    glm::mat4 R = glm::mat4_cast(q);

    glm::mat4 S = glm::scale(glm::mat4(1.0f),
        glm::vec3(node->scale[0], node->scale[1], node->scale[2]));

    return T * R * S;
}

static void collectNodes(const cgltf_node* node,
    const glm::mat4& parent,
    std::vector<NodeDraw>& out)
{
    glm::mat4 world = parent * localMatrix(node);

    if (node->mesh)
    {
        for (size_t pi = 0; pi < node->mesh->primitives_count; ++pi)
        {
            NodeDraw nd{};
            nd.prim = &node->mesh->primitives[pi];
            nd.model = world;
            out.push_back(nd);
        }
    }
    for (size_t c = 0; c < node->children_count; ++c)
    {
        collectNodes(node->children[c], world, out);
    }
}

glTFSampler parse_sampler(const cgltf_texture_view& view)
{
    glTFSampler sampler{};
    if (view.texture->sampler->mag_filter == cgltf_filter_type_nearest)
        sampler.mag_filter = gltfFilter::nearest;
    switch (view.texture->sampler->min_filter)
    {
    case cgltf_filter_type_nearest:
    case cgltf_filter_type_nearest_mipmap_nearest:
        sampler.min_filter = gltfFilter::nearest;
        sampler.mipmap = gltfMipmap::nearest;
        break;
    case cgltf_filter_type_linear:
    case cgltf_filter_type_linear_mipmap_nearest:
        sampler.min_filter = gltfFilter::linear;
        sampler.mipmap = gltfMipmap::nearest;
        break;
    case cgltf_filter_type_nearest_mipmap_linear:
        sampler.min_filter = gltfFilter::nearest;
        sampler.mipmap = gltfMipmap::linear;
        break;
    case cgltf_filter_type_linear_mipmap_linear:
        sampler.min_filter = gltfFilter::linear;
        sampler.mipmap = gltfMipmap::linear;
        break;
    }
    switch (view.texture->sampler->wrap_s)
    {
    case cgltf_wrap_mode_clamp_to_edge: sampler.wrap_u = gltfWrap::clamp_to_edge; break;
    case cgltf_wrap_mode_mirrored_repeat: sampler.wrap_u = gltfWrap::mirrored_repeat; break;
    case cgltf_wrap_mode_repeat: sampler.wrap_u = gltfWrap::repeat; break;
    }
    switch (view.texture->sampler->wrap_t)
    {
    case cgltf_wrap_mode_clamp_to_edge: sampler.wrap_v = gltfWrap::clamp_to_edge; break;
    case cgltf_wrap_mode_mirrored_repeat: sampler.wrap_v = gltfWrap::mirrored_repeat; break;
    case cgltf_wrap_mode_repeat: sampler.wrap_v = gltfWrap::repeat; break;
    }
    return sampler;
}

struct mikkUserData
{
    std::vector<glTFVertex>* vertices;
    const u32* indices;
    i32 faceCount;
};

static int mikk_getNumFaces(const SMikkTSpaceContext* ctx)
{
    const mikkUserData* ud = (const mikkUserData*)ctx->m_pUserData;
    return ud->faceCount;
}

static int mikk_getNumVerticesOfFace(const SMikkTSpaceContext* /*ctx*/, const int /*iFace*/)
{
    return 3;
}

static inline uint32_t mikk_index(const mikkUserData* ud, int iFace, int iVert)
{
    return ud->indices[iFace * 3 + iVert];
}

static void mikk_getPosition(const SMikkTSpaceContext* ctx, float outPos[], const int iFace, const int iVert)
{
    const mikkUserData* ud = (const mikkUserData*)ctx->m_pUserData;
    uint32_t idx = mikk_index(ud, iFace, iVert);
    const glm::vec3& p = (*ud->vertices)[idx].position;
    outPos[0] = p.x; outPos[1] = p.y; outPos[2] = p.z;
}

static void mikk_getNormal(const SMikkTSpaceContext* ctx, float outNorm[], const int iFace, const int iVert)
{
    const mikkUserData* ud = (const mikkUserData*)ctx->m_pUserData;
    uint32_t idx = mikk_index(ud, iFace, iVert);
    glm::vec3 n = glm::normalize((*ud->vertices)[idx].normal);
    outNorm[0] = n.x; outNorm[1] = n.y; outNorm[2] = n.z;
}

static void mikk_getTexCoord(const SMikkTSpaceContext* ctx, float outUV[], const int iFace, const int iVert)
{
    const mikkUserData* ud = (const mikkUserData*)ctx->m_pUserData;
    uint32_t idx = mikk_index(ud, iFace, iVert);
    const glm::vec2& uv = (*ud->vertices)[idx].uv;
    outUV[0] = uv.x; outUV[1] = uv.y;
}

static void mikk_setTSpaceBasic(const SMikkTSpaceContext* ctx,
    const float tangent[3], const float fSign,
    const int iFace, const int iVert)
{
    mikkUserData* ud = (mikkUserData*)ctx->m_pUserData;
    uint32_t idx = mikk_index(ud, iFace, iVert);
    (*ud->vertices)[idx].tangent = glm::vec4(tangent[0], tangent[1], tangent[2], fSign);
}

static void build_tangentsMikk(std::vector<glTFVertex>& vertices, const std::vector<u32>& indices, u32 firstIndex, u32 indexCount)
{
    if (indexCount < 3 || (indexCount % 3) != 0) return;

    for (uint32_t i = 0; i < (uint32_t)vertices.size(); ++i)
    {
        glm::vec3 n = vertices[i].normal;
        float len = glm::length(n);
        if (!(len > 1e-6f)) vertices[i].normal = glm::vec3(0, 0, 1);
        else vertices[i].normal = n / len;
    }

    mikkUserData ud{};
    ud.vertices = &vertices;
    ud.indices = indices.data() + firstIndex;
    ud.faceCount = static_cast<i32>(indexCount / 3);

    SMikkTSpaceInterface iface{};
    iface.m_getNumFaces = mikk_getNumFaces;
    iface.m_getNumVerticesOfFace = mikk_getNumVerticesOfFace;
    iface.m_getPosition = mikk_getPosition;
    iface.m_getNormal = mikk_getNormal;
    iface.m_getTexCoord = mikk_getTexCoord;

    iface.m_setTSpaceBasic = mikk_setTSpaceBasic;
    iface.m_setTSpace = nullptr;

    SMikkTSpaceContext ctx{};
    ctx.m_pInterface = &iface;
    ctx.m_pUserData = &ud;

    genTangSpaceDefault(&ctx);
}

glTFMesh::glTFMesh(const std::string_view path)
{
    cgltf_options options{};
    cgltf_data* data = nullptr;

    if (cgltf_parse_file(&options, path.data(), &data) != cgltf_result_success)
    {
        std::cerr << "Failed to parse glTF: " << path << "\n";
        return;
    }
    if (cgltf_load_buffers(&options, data, path.data()) != cgltf_result_success)
    {
        std::cerr << "Failed to load buffers for glTF\n";
        cgltf_free(data);
        return;
    }

    const std::string baseDir(path.substr(0, path.find_last_of("/\\") + 1));
    auto read_vec3 = [](const cgltf_accessor* acc, cgltf_size i) -> glm::vec3
        {
            f32 v[3] = { 0,0,0 };
            if (acc) cgltf_accessor_read_float(acc, i, v, 3);
            return glm::vec3(v[0], v[1], v[2]);
        };
    auto read_vec2 = [](const cgltf_accessor* acc, cgltf_size i) -> glm::vec2
        {
            f32 v[2] = { 0,0 };
            if (acc) cgltf_accessor_read_float(acc, i, v, 2);
            return glm::vec2(v[0], v[1]);
        };

    // 노드 기반 드로우 리스트 만들기
    std::vector<NodeDraw> draws;
    const cgltf_scene* scene = data->scene ? data->scene : &data->scenes[0];
    for (size_t i = 0; i < scene->nodes_count; ++i)
    {
        collectNodes(scene->nodes[i], mat4(1.f), draws);
    }

    // 모든 primitive 처리
    for (auto& nd : draws)
    {
        const cgltf_primitive& prim = *nd.prim;
        
        const cgltf_accessor* acc_pos = nullptr;
        const cgltf_accessor* acc_nrm = nullptr;
        const cgltf_accessor* acc_tan = nullptr;
        const cgltf_accessor* acc_uv[2] = { nullptr, nullptr };

        for (size_t a = 0; a < prim.attributes_count; ++a)
        {
            const cgltf_attribute& at = prim.attributes[a];
            switch (at.type)
            {
            case cgltf_attribute_type_position: acc_pos = at.data; break;
            case cgltf_attribute_type_normal:   acc_nrm = at.data; break;
            case cgltf_attribute_type_tangent: acc_tan = at.data; break;
            case cgltf_attribute_type_texcoord:
                if (at.index < 2) 
                    acc_uv[at.index] = at.data;
                break;
            default: break;
            }
        }
        if (!acc_pos) continue;

        glTFSubmesh sm{};
        std::string base_path;
        std::string normal_path;
        std::string m_r_path;
        glTFSampler base_sampler{
            .mag_filter = gltfFilter::linear,
            .min_filter = gltfFilter::linear,
            .mipmap = gltfMipmap::linear,
            .wrap_u = gltfWrap::clamp_to_edge,
            .wrap_v = gltfWrap::clamp_to_edge
        };
        glTFSampler norm_sampler{
           .mag_filter = gltfFilter::linear,
           .min_filter = gltfFilter::linear,
           .mipmap = gltfMipmap::linear,
           .wrap_u = gltfWrap::clamp_to_edge,
           .wrap_v = gltfWrap::clamp_to_edge
        };
        glTFSampler m_r_sampler{
           .mag_filter = gltfFilter::linear,
           .min_filter = gltfFilter::linear,
           .mipmap = gltfMipmap::linear,
           .wrap_u = gltfWrap::clamp_to_edge,
           .wrap_v = gltfWrap::clamp_to_edge
        };

        u32 uv_set = 0;
        if (prim.material)
        {
            const auto& pbr = prim.material->pbr_metallic_roughness;
            const cgltf_texture_view& bc = pbr.base_color_texture;
            const cgltf_texture_view& normal = prim.material->normal_texture;
            const cgltf_texture_view& m_r = pbr.metallic_roughness_texture;
            uv_set = (bc.has_transform || bc.texture) ? (u32)bc.texcoord : 0;
            if (prim.material->alpha_mode == cgltf_alpha_mode_blend)
                sm.is_alpha_blend = true;
            if (prim.material->alpha_mode == cgltf_alpha_mode_mask)
                sm.alpha_cutoff = prim.material->alpha_cutoff;
            if (pbr.metallic_factor > 0.f)
                sm.metalic_factor = pbr.metallic_factor;
            if (pbr.roughness_factor > 0.f)
                sm.roughness_factor = pbr.roughness_factor;
            if (prim.material->double_sided > 0)
                sm.is_double_sided = true;

            if (bc.texture && bc.texture->image && bc.texture->image->uri)
                base_path = baseDir + bc.texture->image->uri;
            if (normal.texture && normal.texture->image && normal.texture->image->uri)
                normal_path = baseDir + normal.texture->image->uri;
            if (m_r.texture && m_r.texture->image && m_r.texture->image->uri)
                m_r_path = baseDir + m_r.texture->image->uri;

            if (bc.texture && bc.texture->sampler)
                base_sampler = parse_sampler(bc);
            if (normal.texture && normal.texture->sampler)
                norm_sampler = parse_sampler(normal);
            if (m_r.texture && m_r.texture->sampler)
                m_r_sampler = parse_sampler(m_r);
        }
        const cgltf_accessor* acc_uv_sel =
            (uv_set == 1 && acc_uv[1]) ? acc_uv[1] :
            (acc_uv[0] ? acc_uv[0] : nullptr);

        std::vector<u32> raw;
        if (prim.indices)
        {
            raw.resize(prim.indices->count);
            for (size_t i = 0; i < raw.size(); ++i)
                raw[i] = (u32)cgltf_accessor_read_index(prim.indices, i);
        }
        else
        {
            raw.resize(acc_pos->count);
            for (u32 i = 0; i < (u32)acc_pos->count; ++i) raw[i] = i;
        }

        std::vector<u32> tri;
        tri.reserve(raw.size());
        auto push_tri = [&](u32 a, u32 b, u32 c)
            {
                tri.push_back(a); tri.push_back(b); tri.push_back(c);
            };

        switch (prim.type)
        {
        case cgltf_primitive_type_triangles:
            tri = std::move(raw); break;
        case cgltf_primitive_type_triangle_strip:
            if (raw.size() >= 3) {
                for (size_t i = 2; i < raw.size(); ++i)
                {
                    if ((i & 1) == 0) push_tri(raw[i - 2], raw[i - 1], raw[i]);
                    else              push_tri(raw[i - 1], raw[i - 2], raw[i]);
                }
            } break;
        case cgltf_primitive_type_triangle_fan:
            if (raw.size() >= 3) {
                for (size_t i = 2; i < raw.size(); ++i)
                    push_tri(raw[0], raw[i - 1], raw[i]);
            } break;
        default: continue;
        }

        const u32 baseVertex = (u32)vertices.size();
        const u32 vertexCount = (u32)acc_pos->count;
        vertices.resize(baseVertex + vertexCount);

        for (u32 vi = 0; vi < vertexCount; ++vi)
        {
            glTFVertex v{};
            v.position = read_vec3(acc_pos, vi);
            if (acc_nrm) v.normal = glm::normalize(read_vec3(acc_nrm, vi));
            else v.normal = glm::vec3(0);

            if (acc_uv_sel)
            {
                glm::vec2 uv = read_vec2(acc_uv_sel, vi);
                v.uv = glm::vec2(uv.x, uv.y);
            }
            else v.uv = glm::vec2(0);

            vertices[baseVertex + vi] = v;
        }

        const u32 baseIndex = (u32)indices.size();
        indices.reserve(baseIndex + (u32)tri.size());
        for (u32 id : tri) indices.push_back(baseVertex + id);

        if (!acc_nrm)
        {
            for (size_t t = 0; t + 2 < tri.size(); t += 3)
            {
                u32 i0 = baseVertex + tri[t + 0];
                u32 i1 = baseVertex + tri[t + 1];
                u32 i2 = baseVertex + tri[t + 2];
                const glm::vec3& p0 = vertices[i0].position;
                const glm::vec3& p1 = vertices[i1].position;
                const glm::vec3& p2 = vertices[i2].position;
                glm::vec3 n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
                if (glm::any(glm::isnan(n))) n = glm::vec3(0, 0, 1);
                vertices[i0].normal += n;
                vertices[i1].normal += n;
                vertices[i2].normal += n;
            }
            for (u32 vi = 0; vi < vertexCount; ++vi)
            {
                glm::vec3& n = vertices[baseVertex + vi].normal;
                float len = glm::length(n);
                n = (len > 1e-6f) ? (n / len) : glm::vec3(0, 0, 1);
            }
        }

        if (acc_tan && acc_tan->type == cgltf_type_vec4 &&
            acc_tan->component_type == cgltf_component_type_r_32f)
        {
            for (size_t vi = 0; vi < acc_tan->count; ++vi)
            {
                float t[4] = { 0,0,0,1 };
                cgltf_accessor_read_float(acc_tan, vi, t, 4);
                vertices[baseVertex + (u32)vi].tangent = glm::vec4(t[0], t[1], t[2], t[3]);
            }
        }
        else
        {
            u32 subFirst = baseIndex;
            u32 subCount = static_cast<u32>(indices.size()) - baseIndex;
            build_tangentsMikk(vertices, indices, subFirst, subCount);
        }

        sm.firstIndex = baseIndex;
        sm.indexCount = (u32)indices.size() - baseIndex;
        sm.base_tex = base_path;
        sm.normal_tex = normal_path;
        sm.metalic_roughness_tex = m_r_path;
        sm.base_sampler = base_sampler;
        sm.norm_sampler = norm_sampler;
        sm.m_r_sampler = m_r_sampler;
        sm.model = nd.model;
        submeshes.push_back(std::move(sm));
    }

    cgltf_free(data);
}
