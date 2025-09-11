#pragma once

#include "rhi/rhiRenderpass.h"
#include "rhi/rhiDescriptor.h"
#include "rhi/rhiPipeline.h"

class rhiCommandList;
class renderShared;
class rhiTexture;
class rhiPipeline;
class rhiDeviceContext;

struct drawInitContext
{
    renderShared* rs;
    u32 w;
    u32 h;
    u32 layers = 1;

    virtual std::unique_ptr<drawInitContext> clone() const 
    {
        return std::make_unique<drawInitContext>(*this);
    }
};

struct drawUpdateContext
{
};

class drawPass 
{
public:
    virtual ~drawPass();

    virtual void initialize(const drawInitContext& context);

    void frame(const u32 image_index);
    virtual void resize(renderShared* rs, u32 w, u32 h, u32 layers = 1);
    virtual void shutdown();
    virtual void render(renderShared* rs);

    const u32 get_width() const { return draw_context->w; }
    const u32 get_height() const { return draw_context->h; }
    rhiPipeline* get_pipeline() { return pipeline.get(); }

protected:
    virtual void begin(rhiCommandList* cmd);
    virtual void end(rhiCommandList* cmd);
    virtual void draw(rhiCommandList* cmd) {};
    virtual void begin_barrier(rhiCommandList* cmd) {};
    virtual void end_barrier(rhiCommandList* cmd) {};
    virtual void build_layouts(renderShared* rs) {}
    virtual void build_attachments(rhiDeviceContext* context) {};
    virtual void build_pipeline(renderShared* rs) {};

    void create_pipeline_layout(renderShared* rs, const std::vector<rhiDescriptorSetLayout>& layouts, u32 push_constant_bytes);
    void create_descriptor_sets(renderShared* rs, const std::vector<rhiDescriptorSetLayout>& layouts);

protected:
    std::unique_ptr<drawInitContext> draw_context;
    rhiRenderingInfo render_info;
    std::unique_ptr<rhiPipeline> pipeline;
    rhiPipelineLayout pipeline_layout;
    std::vector<std::vector<rhiDescriptorSet>> descriptor_sets;
    std::vector<u32> dynamic_offsets;
    std::optional<u32> image_index;
    bool initialized = false;
};