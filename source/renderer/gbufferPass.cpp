#include "gbufferPass.h"
#include "renderShared.h"
#include "rhi/rhiCommandList.h"
#include "rhi/rhiDefs.h"
#include "rhi/rhiTextureView.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiBuffer.h"
#include "rhi/rhiBindlessTable.h"
#include "mesh/glTFMesh.h"

void gbufferPass::initialize(const drawInitContext& context)
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
        .is_depth = true,
        .is_separate_depth_stencil = true
    };
    auto tex = context.rs->context->create_texture(td_depth);
    depth.reset(tex.release());

    drawPass::initialize(context);
}

void gbufferPass::resize(renderShared* rs, u32 w, u32 h, u32 layers)
{
    if (!initialized)
        return;

    drawPass::resize(rs, w, h, layers);
    gbuffer_a.reset();
    gbuffer_b.reset();
    gbuffer_c.reset();
    build_attachments(rs->context);
}

void gbufferPass::shutdown()
{
    drawPass::shutdown();
    gbuffer_a.reset();
    gbuffer_b.reset();
    gbuffer_c.reset();
    initialized = false;
}

void gbufferPass::begin(rhiCommandList* cmd)
{
    drawPass::begin(cmd);

    auto ptr = static_cast<gbufferInitContext*>(draw_context.get());
    assert(ptr);

    auto table_ptr = ptr->bindless_table.lock();
    assert(table_ptr);
    table_ptr->bind_once(cmd, pipeline_layout, 2);
}

void gbufferPass::draw(rhiCommandList* cmd)
{
    auto buffer_ptr = indirect_buffer.lock();
    cmd->bind_pipeline(pipeline.get());

    constexpr u32 stride = sizeof(rhiDrawIndexedIndirect);
    for (const auto& g : *group_records)
    {
        push_constants(cmd, g);
        cmd->bind_vertex_buffer(const_cast<rhiBuffer*>(g.vbo), 0, 0);
        cmd->bind_index_buffer(const_cast<rhiBuffer*>(g.ibo), 0);
        const u32 byte_offset = g.first_cmd * stride;
        cmd->draw_indexed_indirect(buffer_ptr.get(), byte_offset, g.cmd_count, stride);
    }
}

void gbufferPass::build_layouts(renderShared* rs)
{
    // gbuffer b0, t0 + s0, t0
    set_globals = rs->context->create_descriptor_set_layout(
        {
            {
                .binding = 0,
                .type = rhiDescriptorType::uniform_buffer,
                .count = 1,
                .stage = rhiShaderStage::vertex | rhiShaderStage::fragment
            },
        }, 0);
    
    set_instances = rs->context->create_descriptor_set_layout(
        {
            {
                .binding = 0,
                .type = rhiDescriptorType::storage_buffer,
                .count = 1,
                .stage = rhiShaderStage::vertex
            },
        }, 1);

    auto ptr = static_cast<gbufferInitContext*>(draw_context.get());
    assert(ptr);

    auto table_ptr = ptr->bindless_table.lock();
    assert(table_ptr);

    set_material = table_ptr->get_set_layout();
    create_pipeline_layout(rs, { set_globals, set_instances, set_material }, sizeof(materialPC));
    create_descriptor_sets(rs, { set_globals, set_instances });
}

void gbufferPass::build_attachments(rhiDeviceContext* context)
{
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

void gbufferPass::build_pipeline(renderShared* rs)
{
    auto vs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\gbuffer.vs.spv");
    auto fs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\gbuffer.ps.spv");
    rhiGraphicsPipelineDesc desc{
        .vs = vs,
        .fs = fs,
        .color_formats = {gbuffer_a->desc.format, gbuffer_b->desc.format, gbuffer_c->desc.format},
        .depth_format = depth->desc.format,
        .samples = rhiSampleCount::x1,
        .depth_test = true,
        .depth_write = true,
        .vertex_layout = rhiVertexAttribute{
            .binding = 0,
            .stride = sizeof(glTFVertex),
            .vertex_attr_desc = {
                rhiVertexAttributeDesc{ 0, 0, rhiFormat::RGB32_SFLOAT, offsetof(glTFVertex, position) },
                rhiVertexAttributeDesc{ 1, 0, rhiFormat::RGB32_SFLOAT, offsetof(glTFVertex, normal) },
                rhiVertexAttributeDesc{ 2, 0, rhiFormat::RG32_SFLOAT, offsetof(glTFVertex, uv) },
                rhiVertexAttributeDesc{ 3, 0, rhiFormat::RGBA32_SFLOAT, offsetof(glTFVertex, tangent) },
        }}
    };
    pipeline = rs->context->create_graphics_pipeline(desc, pipeline_layout);

    shaderio::free_shader_binary(vs);
    shaderio::free_shader_binary(fs);
}

void gbufferPass::begin_barrier(rhiCommandList* cmd)
{
    if (is_first_frame)
    {
        cmd->image_barrier(gbuffer_a.get(), rhiImageLayout::undefined, rhiImageLayout::color_attachment, 0, 1, 0, gbuffer_a->desc.layers);
        cmd->image_barrier(gbuffer_b.get(), rhiImageLayout::undefined, rhiImageLayout::color_attachment, 0, 1, 0, gbuffer_b->desc.layers);
        cmd->image_barrier(gbuffer_c.get(), rhiImageLayout::undefined, rhiImageLayout::color_attachment, 0, 1, 0, gbuffer_c->desc.layers);
        cmd->image_barrier(depth.get(), rhiImageLayout::undefined, rhiImageLayout::depth_stencil_attachment, 0, 1, 0, depth->desc.layers);
        is_first_frame = false;
    }
    else
    {
        cmd->image_barrier(gbuffer_a.get(), rhiImageLayout::shader_readonly, rhiImageLayout::color_attachment, 0, 1, 0, gbuffer_a->desc.layers);
        cmd->image_barrier(gbuffer_b.get(), rhiImageLayout::shader_readonly, rhiImageLayout::color_attachment, 0, 1, 0, gbuffer_b->desc.layers);
        cmd->image_barrier(gbuffer_c.get(), rhiImageLayout::shader_readonly, rhiImageLayout::color_attachment, 0, 1, 0, gbuffer_c->desc.layers);
        cmd->image_barrier(depth.get(), rhiImageLayout::shader_readonly, rhiImageLayout::depth_stencil_attachment, 0, 1, 0, depth->desc.layers);
    }
}

void gbufferPass::end_barrier(rhiCommandList* cmd)
{
    cmd->image_barrier(gbuffer_a.get(), rhiImageLayout::color_attachment, rhiImageLayout::shader_readonly, 0, 1, 0, gbuffer_a->desc.layers);
    cmd->image_barrier(gbuffer_b.get(), rhiImageLayout::color_attachment, rhiImageLayout::shader_readonly, 0, 1, 0, gbuffer_b->desc.layers);
    cmd->image_barrier(gbuffer_c.get(), rhiImageLayout::color_attachment, rhiImageLayout::shader_readonly, 0, 1, 0, gbuffer_c->desc.layers);
    cmd->image_barrier(depth.get(), rhiImageLayout::depth_stencil_attachment, rhiImageLayout::shader_readonly, 0, 1, 0, depth->desc.layers);
}

void gbufferPass::update(renderShared* rs, const rhiBuffer* global_buffer, const rhiBuffer* instance_storage_buf)
{
    update_globals(rs, global_buffer, 0);
    update_instances(rs, 1);
}

void gbufferPass::update_globals(renderShared* rs, const rhiBuffer* global_buffer, const u32 offset)
{
    if (!global_buffer)
        return;

    const rhiDescriptorBufferInfo buffer_info{
        .buffer = const_cast<rhiBuffer*>(global_buffer),
        .offset = offset,
        .range = sizeof(globalsCB)
    };
    const rhiWriteDescriptor write_desc{
        .set = descriptor_sets[image_index.value()][0],
        .binding = 0,
        .array_index = 0,
        .count = 1,
        .type = rhiDescriptorType::uniform_buffer,
        .buffer = { buffer_info }
    };
    rs->context->update_descriptors({ write_desc });
}

void gbufferPass::push_constants(rhiCommandList* cmd, const groupRecord& g)
{
    materialPC pc = g;
    cmd->push_constants(pipeline_layout, rhiShaderStage::all, 0, sizeof(pc), &pc);
}