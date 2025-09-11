#pragma once

#include "indirectDrawPass.h"

class rhiBindlessTable;
class rhiDeviceContext;
struct rhiBindlessHandle;

struct gbufferInitContext : public drawInitContext
{
    std::weak_ptr<rhiBindlessTable> bindless_table;
    std::unique_ptr<drawInitContext> clone() const override
    {
        return std::make_unique<gbufferInitContext>(*this);
    }
};

class gbufferPass final : public indirectDrawPass
{
private:
    struct alignas(16) materialPC
    {
        u32 base_color_index;
        u32 norm_color_index;
        u32 mr_color_index;
        u32 base_sampler_index;
        u32 norm_sampler_index;
        u32 mr_sampler_index;

        f32 alpha_cutoff;
        f32 metalic_factor;
        f32 roughness_factor;
        f32 __pad[3];

        materialPC(const groupRecord& g)
            : base_color_index(g.base_color_index),
            norm_color_index(g.norm_color_index),
            mr_color_index(g.m_r_color_index),
            base_sampler_index(g.base_sam_index),
            norm_sampler_index(g.norm_sam_index),
            mr_sampler_index(g.m_r_sam_index),
            alpha_cutoff(g.alpha_cutoff),
            metalic_factor(g.metalic_factor),
            roughness_factor(g.roughness_factor) {}
    };

public:
    void initialize(const drawInitContext& context) override;
    void resize(renderShared* rs, u32 w, u32 h, u32 layers = 1) override;
    void shutdown() override;
    void begin(rhiCommandList* cmd) override;
    void draw(rhiCommandList* cmd) override;
    void begin_barrier(rhiCommandList* cmd) override;
    void end_barrier(rhiCommandList* cmd) override;

    void update(renderShared* rs, const rhiBuffer* global_buffer, const rhiBuffer* instance_storage_buf);
    void push_constants(rhiCommandList* cmd, const groupRecord& g);

    rhiTexture* get_gbuffer_a() const { return gbuffer_a.get(); }
    rhiTexture* get_gbuffer_b() const { return gbuffer_b.get(); }
    rhiTexture* get_gbuffer_c() const { return gbuffer_c.get(); }
    rhiTexture* get_depth() const { return depth.get(); }

protected:
    void build_layouts(renderShared* rs) override;
    void build_attachments(rhiDeviceContext* context) override;
    void build_pipeline(renderShared* rs) override;

private:
    void update_globals(renderShared* rs, const rhiBuffer* global_buffer, const u32 offset);

private:
    std::unique_ptr<rhiTexture> gbuffer_a;
    std::unique_ptr<rhiTexture> gbuffer_b;
    std::unique_ptr<rhiTexture> gbuffer_c;
    std::unique_ptr<rhiTexture> depth;


    // set=0: Globals (b0)
    rhiDescriptorSetLayout set_globals;
    // set=1: Material (t0 + s0)
    rhiDescriptorSetLayout set_material;
    // set=2: Instances (t0)
    rhiDescriptorSetLayout set_instances;
    
    bool is_first_frame = true;
};
