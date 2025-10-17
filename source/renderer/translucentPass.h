#pragma once

#include "indirectDrawPass.h"

class rhiBindlessTable;
struct translucentInitContext : public drawInitContext
{
    std::weak_ptr<rhiBindlessTable> bindless_table;
    rhiTexture* depth;
#if DISABLE_OIT
    std::weak_ptr<rhiTexture> scene_color;
#endif
    std::unique_ptr<drawInitContext> clone() const override
    {
        return std::make_unique<translucentInitContext>(*this);
    }
};

struct translucentUpdateContext : public drawUpdateContext
{
    rhiBuffer* global_buffer;
    rhiTexture* shadow_depth;
    std::vector<mat4> light_viewproj;
    vec4 light_dir;
    std::vector<f32> cascade_splits;
    f32 shadow_mapsize;
};

class translucentPass final : public indirectDrawPass
{
private:
    struct alignas(16) lightCB
    {
        mat4 light_viewproj[4]; // 256b.. todo dynamic cascade count....
        vec4 light_dir; // 16b
        vec4 cascade_splits; // 16b
        vec2 shadow_mapsize; // 8b
        vec2 _pad; // 8b
    }; // 304b

public:
    void initialize(const drawInitContext& context) override;
    void update(drawUpdateContext* update_context) override;

protected:
    void begin(rhiCommandList* cmd) override;
    void draw(rhiCommandList* cmd) override;
    void begin_barrier(rhiCommandList* cmd) override;
    void end_barrier(rhiCommandList* cmd) override;
    void build_layouts(renderShared* rs) override;
    void build_attachments(rhiDeviceContext* context) override;
    void build_pipeline(renderShared* rs) override;

public:
    rhiTexture* get_accum() { return accumulate_color_alpha.get(); }
    rhiTexture* get_reveal() { return revealage.get(); }

private:
    void update_buffer(translucentUpdateContext* update_context);
    void push_constants(rhiCommandList* cmd, const groupRecord& g);

private:
    std::unique_ptr<rhiTexture> accumulate_color_alpha;
    std::unique_ptr<rhiTexture> revealage;
    std::vector<std::unique_ptr<rhiBuffer>> light_ring_buffer;
    rhiDescriptorSetLayout set_globals;
    rhiDescriptorSetLayout set_instances;
    rhiDescriptorSetLayout set_light;
    rhiDescriptorSetLayout set_material;
};