#pragma once

#include "drawPass.h"

struct rhiRenderTargetView;
class rhiFrameContext;

struct compositeInitContext : public drawInitContext
{
    std::vector<rhiRenderTargetView> swapchain_views;

    virtual std::unique_ptr<drawInitContext> clone() const
    {
        return std::make_unique<compositeInitContext>(*this);
    }
};

class compositePass final : public drawPass
{
public:
    struct compositeCB 
    { 
        vec2 inv_rt;
        float exposure;
        float pad; 
    };

public:
    void initialize(const drawInitContext& context) override;
    void resize(renderShared* rs, u32 w, u32 h, u32 layers = 1) override;
    void shutdown() override;
    void update(renderShared* rs, rhiTexture* scene_color);

protected:
    void draw(rhiCommandList* cmd) override;
    void begin_barrier(rhiCommandList* cmd) override;
    void end_barrier(rhiCommandList* cmd) override;
    void build_layouts(renderShared* rs) override;
    void build_pipeline(renderShared* rs) override;

private:
    void set_swapchain_views(const std::vector<rhiRenderTargetView>& views);
    void create_composite_cbuffer(renderShared* rs);
    void update_descriptors(renderShared* rs, rhiTexture* scene_color);
    void update_renderinfo(renderShared* rs);
    void update_cbuffer(renderShared* rs);

private:
    std::vector<rhiRenderTargetView> swapchain_views;
    std::vector<std::unique_ptr<rhiBuffer>> composite_buffers;

    // full screen or compisite ( [0]=composite set )
    rhiDescriptorSetLayout set_composite;
    std::vector<bool> is_first_frames;
};
