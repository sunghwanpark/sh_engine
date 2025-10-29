#include "compositePass.h"
#include "renderShared.h"
#include "rhi/rhiCommandList.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiTextureView.h"
#include "rhi/rhiFrameContext.h"

void compositePass::initialize(const drawInitContext& context)
{
    drawPass::initialize(context);
    auto ptr = static_cast<compositeInitContext*>(init_context.get());
    ASSERT(ptr);
    set_swapchain_views(ptr->swapchain_views);
    create_composite_cbuffer(ptr->rs);
}

void compositePass::resize(renderShared* rs, u32 w, u32 h, u32 layers)
{
    drawPass::resize(rs, w, h, layers);
}

void compositePass::shutdown()
{
}

void compositePass::draw(rhiCommandList* cmd)
{
    cmd->draw_fullscreen();
}

void compositePass::begin_barrier(rhiCommandList* cmd)
{
    if (is_first_frames[image_index.value()])
    {
        cmd->image_barrier(swapchain_views[image_index.value()].texture, rhiImageLayout::undefined, rhiImageLayout::color_attachment, 0, 1, 0, 1, true);
        is_first_frames[image_index.value()] = false;
    }
    else
        cmd->image_barrier(swapchain_views[image_index.value()].texture, rhiImageLayout::present, rhiImageLayout::color_attachment, 0, 1, 0, 1, true);
}

void compositePass::end_barrier(rhiCommandList* cmd)
{
    cmd->image_barrier(swapchain_views[image_index.value()].texture, rhiImageLayout::color_attachment, rhiImageLayout::present);
}

void compositePass::set_swapchain_views(const std::vector<rhiRenderTargetView>& views)
{
	swapchain_views = views;
    is_first_frames.resize(views.size(), true);
}

void compositePass::build_layouts(renderShared* rs)
{
    set_composite = rs->context->create_descriptor_set_layout(
        {
            {
                .binding = 0,
                .type = rhiDescriptorType::uniform_buffer,
                .count = 1,
                .stage = rhiShaderStage::vertex | rhiShaderStage::fragment
            },
            {
                .binding = 1,
                .type = rhiDescriptorType::sampled_image,
                .count = 1,
                .stage = rhiShaderStage::fragment
            },
            {
                .binding = 2,
                .type = rhiDescriptorType::sampler,
                .count = 1,
                .stage = rhiShaderStage::fragment
            },
        }, 0);

    create_pipeline_layout(rs, { set_composite });
    create_descriptor_sets(rs, { set_composite });
}

void compositePass::build_pipeline(renderShared* rs)
{
    auto vs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\fullscreen.vs.spv");
    auto fs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\composite.ps.spv");
    const rhiGraphicsPipelineDesc pipeline_desc{
        .vs = vs,
        .fs = fs,
        .color_formats = { rhiFormat::RGBA8_SRGB },
        .depth_format = std::nullopt,
        .samples = rhiSampleCount::x1,
        .depth_test = false,
        .depth_write = false
    };

    pipeline = rs->context->create_graphics_pipeline(pipeline_desc, pipeline_layout);

    shaderio::free_shader_binary(vs);
    shaderio::free_shader_binary(fs);
}

void compositePass::update(renderShared* rs, rhiTexture* scene_color)
{
    update_renderinfo(rs);
    update_descriptors(rs, scene_color);
    update_cbuffer(rs);
}

void compositePass::update_descriptors(renderShared* rs, rhiTexture* scene_color)
{
    rhiBuffer* constant_buf = composite_buffers[image_index.value()].get();
    const rhiDescriptorBufferInfo b{
        .buffer = constant_buf,
        .offset = 0,
        .range = sizeof(compositeCB)
    };

    const rhiWriteDescriptor cb_write_desc{
        .set = descriptor_sets[image_index.value()][0] ,
        .binding = 0,
        .array_index = 0,
        .count = 1,
        .type = rhiDescriptorType::uniform_buffer,
        .buffer = { b }
    };

    const rhiWriteDescriptor sc_write_desc{
        .set = descriptor_sets[image_index.value()][0],
        .binding = 1,
        .array_index = 0,
        .count = 1,
        .type = rhiDescriptorType::sampled_image,
        .image = { rhiDescriptorImageInfo{
                .sampler = nullptr,
                .texture = scene_color,
                .mip = 0,
                .base_layer = 0,
                .layer_count = 1,
                .layout = rhiImageLayout::shader_readonly
            }
    } };

    const rhiDescriptorImageInfo sampler_image_info{
        .sampler = rs->samplers.linear_clamp.get()
    };
    const rhiWriteDescriptor sampler_write_desc{
        .set = descriptor_sets[image_index.value()][0],
        .binding = 2,
        .array_index = 0,
        .count = 1,
        .type = rhiDescriptorType::sampler,
        .image = { sampler_image_info }
    };

    rs->context->update_descriptors({ cb_write_desc, sc_write_desc, sampler_write_desc });
}

void compositePass::update_renderinfo(renderShared* rs)
{
    render_info.color_formats = { swapchain_views[image_index.value()].texture->desc.format};
    render_info.color_attachments =
    {
        rhiRenderingAttachment{
            .view = swapchain_views[image_index.value()],
            .load_op = rhiLoadOp::clear,
            .store_op = rhiStoreOp::store,
            .clear = { {1,0,0,1}, 1.0f, 0 }
        }
    };
}

void compositePass::build_attachments(rhiDeviceContext* context)
{
    render_info.renderpass_name = "composite";
    render_info.depth_attachment = std::nullopt;
}

void compositePass::update_cbuffer(renderShared * rs)
{
    const compositeCB ccb{
        .inv_rt = { 1.0f / init_context->w, 1.0f / init_context->h },
        .exposure = 1.f,
        .pad = 0.f
    };
    rs->buffer_barrier(composite_buffers[image_index.value()].get(), rhiBufferBarrierDescription{
        .src_stage = rhiPipelineStage::fragment_shader,
        .dst_stage = rhiPipelineStage::transfer,
        .src_access = rhiAccessFlags::uniform_read,
        .dst_access = rhiAccessFlags::transfer_write,
        .size = sizeof(compositeCB),
        .src_queue = rs->context->get_queue_family_index(rhiQueueType::graphics),
        .dst_queue = rs->context->get_queue_family_index(rhiQueueType::transfer)
        });
    rs->upload_to_device(composite_buffers[image_index.value()].get(), &ccb, sizeof(compositeCB));
    rs->buffer_barrier(composite_buffers[image_index.value()].get(), rhiBufferBarrierDescription{
        .src_stage = rhiPipelineStage::transfer,
        .dst_stage = rhiPipelineStage::fragment_shader,
        .src_access = rhiAccessFlags::transfer_write,
        .dst_access = rhiAccessFlags::uniform_read,
        .size = sizeof(compositeCB),
        .src_queue = rs->context->get_queue_family_index(rhiQueueType::graphics),
        .dst_queue = rs->context->get_queue_family_index(rhiQueueType::transfer)
        });
}

void compositePass::create_composite_cbuffer(renderShared* rs)
{
    if (composite_buffers.size() > 0)
        return;

    composite_buffers.resize(swapchain_views.size());
    std::ranges::for_each(composite_buffers, [&](std::unique_ptr<rhiBuffer>& buf)
        {
            rs->create_or_resize_buffer(buf, sizeof(compositeCB), rhiBufferUsage::uniform | rhiBufferUsage::transfer_dst, rhiMem::auto_device);
        });
}