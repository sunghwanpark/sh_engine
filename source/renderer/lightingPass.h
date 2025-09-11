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

    struct context
    {
        rhiTexture* scene_color;
        rhiTexture* gbuf_a;
        rhiTexture* gbuf_b;
        rhiTexture* gbuf_c;
        rhiTexture* depth;
        rhiTexture* shadows;
    };

public:
    void initialize(const drawInitContext& context) override;
    void begin_barrier(rhiCommandList* cmd) override;
    void end_barrier(rhiCommandList* cmd) override;
    void draw(rhiCommandList* cmd) override;

public:
    void link_textures(context& context);
    void update(renderShared* rs, rhiCommandList* cmd, camera* camera, const vec3& light_dir, const std::vector<mat4>& light_viewprojs, const std::vector<f32>& cascade_splits, const u32 shadow_resolution);

protected:
    void build_layouts(renderShared* rs) override;
    void build_attachments(rhiDeviceContext* context) override;
    void build_pipeline(renderShared* rs) override;

private:
    void update_cbuffers(renderShared* rs, rhiCommandList* cmd, camera* camera, const vec3& light_dir, const std::vector<mat4>& light_viewprojs, const std::vector<f32>& cascade_splits, const u32 shadow_resolution);
    void update_descriptors(renderShared* rs);

private:
    rhiDescriptorSetLayout set_fragment_layout;
    
    // reference ptr
	rhiTexture* scene_color;
    rhiTexture* gbuf_a;
    rhiTexture* gbuf_b;
    rhiTexture* gbuf_c;
    rhiTexture* depth;
    rhiTexture* shadows;
    //

    std::vector<std::unique_ptr<rhiBuffer>> camera_cbuffer;
    std::vector<std::unique_ptr<rhiBuffer>> light_cbuffer;
    bool is_first_frame = true;
};