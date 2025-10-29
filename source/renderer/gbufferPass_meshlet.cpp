#include "gbufferPass_meshlet.h"
#include "renderShared.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiCommandList.h"

void gbufferPass_meshlet::initialize(const drawInitContext& context)
{
    rhiTextureDesc td_color
    {
        .width = context.w,
        .height = context.h,
        .layers = context.layers,
        .mips = 1,
        .format = rhiFormat::RGBA8_UNORM,
        .samples = rhiSampleCount::x1,
        .is_depth = false
    };
    // gbuffer A
    {
        auto tex = context.rs->context->create_texture(td_color);
        gbuffer_a.reset(tex.release());
    }
    // gbuffer B
    {
        auto tex = context.rs->context->create_texture(td_color);
        gbuffer_b.reset(tex.release());
    }
    // gbuffer C
    {
        auto tex = context.rs->context->create_texture(td_color);
        gbuffer_c.reset(tex.release());
    }
    // Depth
    const rhiTextureDesc td_depth
    {
        .width = context.w,
        .height = context.h,
        .layers = context.layers,
        .mips = 1,
        .format = rhiFormat::D32S8,
        .samples = rhiSampleCount::x1,
        .usage = rhiTextureUsage::ds,
        .is_depth = true,
        .is_separate_depth_stencil = true
    };
    auto tex = context.rs->context->create_texture(td_depth);
    depth.reset(tex.release());

    drawPass::initialize(context);
}

void gbufferPass_meshlet::build_layouts(renderShared* rs)
{
    layout_globals = rs->context->create_descriptor_set_layout(
        {
            {
                .binding = 0,
                .type = rhiDescriptorType::uniform_buffer,
                .count = 1,
                .stage = rhiShaderStage::mesh
            },
        }, 0);

    build_meshlet_descriptor_layout(1);

    auto ptr = static_cast<gbufferPass_meshletInitContext*>(init_context.get());
    ASSERT(ptr);

    auto table_ptr = ptr->bindless_table.lock();
    ASSERT(table_ptr);

    layout_material = table_ptr->get_set_layout();
    create_pipeline_layout(rs, { layout_globals, layout_meshlet, layout_material },
        { 
            { rhiShaderStage::mesh | rhiShaderStage::fragment, sizeof(drawParamIndex) + sizeof(materialPC) }
        });
    create_descriptor_sets(rs, { layout_globals, layout_meshlet });
}

void gbufferPass_meshlet::build_attachments(rhiDeviceContext* context)
{
    render_info.renderpass_name = "gbuffer_meshlet";
    render_info.samples = rhiSampleCount::x1;
    render_info.color_formats = { rhiFormat::RGBA8_UNORM, rhiFormat::RGBA8_UNORM, rhiFormat::RGBA8_UNORM };
    render_info.depth_format = rhiFormat::D32S8;

    // Color attachments
    render_info.color_attachments.resize(3);
    {
        rhiRenderTargetView rtview{
            .texture = gbuffer_a.get(),
            .mip = 0,
            .layout = rhiImageLayout::color_attachment
        };
        render_info.color_attachments[0].view = rtview;
    }
    {
        rhiRenderTargetView rtview{
            .texture = gbuffer_b.get(),
            .mip = 0,
            .layout = rhiImageLayout::color_attachment
        };
        render_info.color_attachments[1].view = rtview;
    }
    {
        rhiRenderTargetView rtview{
            .texture = gbuffer_c.get(),
            .mip = 0,
            .layout = rhiImageLayout::color_attachment
        };
        render_info.color_attachments[2].view = rtview;
    }
    for (u32 index = 0; index < 3; ++index)
    {
        render_info.color_attachments[index].load_op = rhiLoadOp::clear;
        render_info.color_attachments[index].store_op = rhiStoreOp::store;
        render_info.color_attachments[index].clear = { {0,0,0,1},1.0f,0 };
    }

    // Depth
    rhiRenderTargetView d{
        .texture = depth.get(),
        .mip = 0,
        .layout = rhiImageLayout::depth_stencil_attachment
    };
    rhiRenderingAttachment depth_attachment;
    depth_attachment.view = d;
    depth_attachment.load_op = rhiLoadOp::clear;
    depth_attachment.store_op = rhiStoreOp::store;
    depth_attachment.clear.depth = 1.f;
    depth_attachment.clear.stencil = 0;
    render_info.depth_attachment = depth_attachment;
}

void gbufferPass_meshlet::build_pipeline(renderShared* rs)
{
    auto ms = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\gbuffer.ms.spv");
    auto fs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\gbuffer_meshlet.ps.spv");
    rhiGraphicsPipelineDesc desc{
        .fs = fs,
        .ms = ms,
        .color_formats = {gbuffer_a->desc.format, gbuffer_b->desc.format, gbuffer_c->desc.format},
        .depth_format = depth->desc.format,
        .samples = rhiSampleCount::x1,
        .depth_test = true,
        .depth_write = true
    };
    pipeline = rs->context->create_graphics_pipeline(desc, pipeline_layout);

    shaderio::free_shader_binary(ms);
    shaderio::free_shader_binary(fs);
}

void gbufferPass_meshlet::begin(rhiCommandList* cmd)
{
    drawPass::begin(cmd);

    auto ptr = static_cast<gbufferPass_meshletInitContext*>(init_context.get());
    ASSERT(ptr);

    auto table_ptr = ptr->bindless_table.lock();
    ASSERT(table_ptr);
    table_ptr->bind_once(cmd, pipeline_layout, 2);
}

void gbufferPass_meshlet::draw(rhiCommandList* cmd)
{
    auto buffer_ptr = indirect_buffer->at(static_cast<u8>(draw_type));
    auto group_record = group_records->at(static_cast<u8>(draw_type));
    cmd->bind_pipeline(pipeline.get());

    constexpr u32 stride = sizeof(rhiDrawMeshShaderIndirect);
    for (const auto& g : group_record)
    {
        drawParamIndex idx{
            .dp_index = g.first_cmd
        };
        cmd->push_constants(pipeline_layout, rhiShaderStage::mesh | rhiShaderStage::fragment, 0, sizeof(idx), &idx);

        materialPC pc = g;
        cmd->push_constants(pipeline_layout, rhiShaderStage::mesh | rhiShaderStage::fragment, sizeof(drawParamIndex), sizeof(pc), &pc);

        const u32 byte_offset = g.first_cmd * stride;
        cmd->draw_mesh_tasks_indirect(buffer_ptr.get(), byte_offset, g.cmd_count, stride);
    }
}

void gbufferPass_meshlet::begin_barrier(rhiCommandList* cmd)
{
    if (is_first_frame)
    {
        cmd->image_barrier(gbuffer_a.get(), rhiImageLayout::undefined, rhiImageLayout::color_attachment, 0, 1, 0, gbuffer_a->desc.layers);
        cmd->image_barrier(gbuffer_b.get(), rhiImageLayout::undefined, rhiImageLayout::color_attachment, 0, 1, 0, gbuffer_b->desc.layers);
        cmd->image_barrier(gbuffer_c.get(), rhiImageLayout::undefined, rhiImageLayout::color_attachment, 0, 1, 0, gbuffer_c->desc.layers);
        cmd->image_barrier(depth.get(), rhiImageLayout::undefined, rhiImageLayout::depth_stencil_attachment, 0, 1, 0, depth->desc.layers);
    }
    else
    {
        cmd->image_barrier(gbuffer_a.get(), rhiImageLayout::shader_readonly, rhiImageLayout::color_attachment, 0, 1, 0, gbuffer_a->desc.layers);
        cmd->image_barrier(gbuffer_b.get(), rhiImageLayout::shader_readonly, rhiImageLayout::color_attachment, 0, 1, 0, gbuffer_b->desc.layers);
        cmd->image_barrier(gbuffer_c.get(), rhiImageLayout::shader_readonly, rhiImageLayout::color_attachment, 0, 1, 0, gbuffer_c->desc.layers);
        cmd->image_barrier(depth.get(), rhiImageLayout::shader_readonly, rhiImageLayout::depth_stencil_attachment, 0, 1, 0, depth->desc.layers);
    }
}

void gbufferPass_meshlet::end_barrier(rhiCommandList* cmd)
{
    cmd->image_barrier(gbuffer_a.get(), rhiImageLayout::color_attachment, rhiImageLayout::shader_readonly, 0, 1, 0, gbuffer_a->desc.layers);
    cmd->image_barrier(gbuffer_b.get(), rhiImageLayout::color_attachment, rhiImageLayout::shader_readonly, 0, 1, 0, gbuffer_b->desc.layers);
    cmd->image_barrier(gbuffer_c.get(), rhiImageLayout::color_attachment, rhiImageLayout::shader_readonly, 0, 1, 0, gbuffer_c->desc.layers);
    cmd->image_barrier(depth.get(), rhiImageLayout::depth_stencil_attachment, rhiImageLayout::shader_readonly, 0, 1, 0, depth->desc.layers);
}
