#pragma once

#include "drawPass.h"

class camera;
class scene;

class lightingPass final : public drawPass
{
public:
    struct alignas(16) cam
    {
        mat4 inv_proj;                  // 64
        mat4 inv_view;                  // 64
        vec2 near_far;                   // 8
        vec2 _pad;                       // 8
        vec4 cascade_splits;            // 16
        vec3 light_dir;                     // 12
        f32 depth_bias = 0.001f;        // 4
        f32 normal_bias = 0.000f;       // 4
        f32 cascade_blend = 0.01f;      // 4
        vec2 shadow_mapsize;            // 8
    };

    struct alignas(16) light
    {
        mat4 light_viewproj[4];
    };

    struct alignas(16) iblParams
    {
        f32 ibl_intensity_diffuse = 1.f;
        f32 ibl_intensity_specular = 1.f;
        f32 specular_mip_count;
        f32 _pad;
    };

    struct textureContext
    {
        rhiTexture* scene_color;
        rhiTexture* gbuf_a;
        rhiTexture* gbuf_b;
        rhiTexture* gbuf_c;
        rhiTexture* depth;
        rhiTexture* shadows;
        rhiTextureCubeMap* ibl_irradiance;
        rhiTextureCubeMap* ibl_specular;
        rhiTexture* ibl_brdf_lut;
    };

public:
    void initialize(const drawInitContext& context) override;
    void begin_barrier(rhiCommandList* cmd) override;
    void end_barrier(rhiCommandList* cmd) override;
    void draw(rhiCommandList* cmd) override;

public:
    void link_textures(textureContext& context);
    void update(renderShared* rs, rhiCommandList* cmd, camera* camera, const vec3& light_dir, const std::vector<mat4>& light_viewprojs, const std::vector<f32>& cascade_splits, const u32 shadow_resolution, const u32 cubemap_mipcount);

protected:
    void build_layouts(renderShared* rs) override;
    void build_attachments(rhiDeviceContext* context) override;
    void build_pipeline(renderShared* rs) override;

private:
    void update_cbuffers(renderShared* rs, rhiCommandList* cmd, camera* camera, const vec3& light_dir, const std::vector<mat4>& light_viewprojs, const std::vector<f32>& cascade_splits, const u32 shadow_resolution, const u32 cubemap_mipcount);
    void update_descriptors(renderShared* rs);

private:
    rhiDescriptorSetLayout set_fragment_layout;
    
    // reference ptr
    textureContext texture_context;
    //

    std::vector<std::unique_ptr<rhiBuffer>> camera_cbuffer;
    std::vector<std::unique_ptr<rhiBuffer>> light_cbuffer;
    std::vector<std::unique_ptr<rhiBuffer>> ibl_param_cbuffer;
    bool is_first_frame = true;
};