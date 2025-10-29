#include "drawPass.h"
#include "renderShared.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiFrameContext.h"
#include "rhi/rhiCommandList.h"
#include "rhi/rhiRenderpass.h"

drawPass::~drawPass()
{
    shutdown();
}


void drawPass::initialize(const drawInitContext& context)
{
    init_context = context.clone();
    render_info.samples = rhiSampleCount::x1;
    render_info.render_area = { 0, 0, init_context->w, init_context->h };

    descriptor_sets.resize(init_context->rs->frame_context->get_frame_size());
    build_layouts(init_context->rs);
    build_attachments(init_context->rs->context);
    build_pipeline(init_context->rs);
}

void drawPass::frame(const u32 image_index)
{
    this->image_index = image_index;
}

void drawPass::resize(renderShared* rs, u32 w, u32 h, u32 layers)
{
    init_context->w = w;
    init_context->h = h;
    init_context->layers = layers;
    render_info.render_area = { 0, 0, init_context->w, init_context->h };
}

void drawPass::shutdown()
{
    pipeline.reset();
    descriptor_sets.clear();
}

void drawPass::render(renderShared* rs)
{
    auto cmd_list = rs->frame_context->get_command_list(main_job_queue);

    begin(cmd_list);
    draw(cmd_list);
    end(cmd_list);

    is_first_frame = false;
}

void drawPass::begin(rhiCommandList* cmd)
{
    begin_barrier(cmd);

    cmd->begin_render_pass(render_info);
    cmd->bind_pipeline(pipeline.get());
    cmd->bind_descriptor_sets(pipeline_layout, rhiPipelineType::graphics, descriptor_sets[image_index.value()], 0, dynamic_offsets);
    cmd->set_viewport_scissor(vec2(init_context->w, init_context->h));
}

void drawPass::end(rhiCommandList* cmd)
{
    cmd->end_render_pass();;
    end_barrier(cmd);
    image_index.reset();
}

void drawPass::create_pipeline_layout(renderShared* rs, const std::vector<rhiDescriptorSetLayout>& layouts, const std::vector<rhiPushConstant>& push_constant_bytes)
{
    pipeline_layout = rs->context->create_pipeline_layout(layouts, push_constant_bytes, nullptr);
}

void drawPass::create_descriptor_sets(renderShared* rs, const std::vector<rhiDescriptorSetLayout>& layouts)
{
    const u32 frame_size = rs->get_frame_size();
    for (u32 index = 0; index < frame_size; ++index)
    {
        auto& desc_pool = rs->arena.pools[index];
        descriptor_sets[index] = rs->context->allocate_descriptor_sets(desc_pool, layouts);
    }
}