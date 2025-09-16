#include "skyPass.h"
#include "renderShared.h"
#include "rhi/rhiCommandList.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiTextureView.h"
#include "rhi/rhiFrameContext.h"
#include "scene/camera.h"

namespace
{
    constexpr i32 cube_resolution = 512;
    constexpr u32 cube_face_count = 6;
    constexpr u32 dispatch_localgroupsize = 8;
}

void skyPass::initialize(const drawInitContext& context)
{
    drawPass::initialize(context);
    fill_skycubemap();
}

void skyPass::fill_skycubemap()
{
    auto rs = draw_context->rs;
    const u32 cube_mip = static_cast<u32>(std::floor(std::log2(cube_resolution))) + 1;

    // hdr to equirect
    auto equirect_tex = rs->context->create_texture_from_path("E:\\Sponza\\resource\\sponza\\qwantani_moonrise_puresky_4k.hdr", true);
    //auto equirect_tex = rs->context->create_texture_from_path("E:\\Sponza\\resource\\sponza\\818-hdri-skies-com.hdr", true);

    // create compute pipeline layout
    auto cs_descriptor_layout1 = rs->context->create_descriptor_set_layout(
        {
            {
                .binding = 0,
                .type = rhiDescriptorType::sampled_image,
                .count = 1,
                .stage = rhiShaderStage::compute
            },
            {
                .binding = 1,
                .type = rhiDescriptorType::sampler,
                .count = 1,
                .stage = rhiShaderStage::compute
            }
        }
    );
    auto cs_descriptor_layout2 = rs->context->create_descriptor_indexing_set_layout(
        {
            .elements = {
                {
                    .binding = 0,
                    .type = rhiDescriptorType::storage_image,
                    .descriptor_count = cube_mip,
                    .stage = rhiShaderStage::compute,
                    .binding_flags = rhiDescriptorBindingFlags::variable_descriptor_count | rhiDescriptorBindingFlags::partially_bound | rhiDescriptorBindingFlags::update_after_bind,
                }
            },
        }, 1);
    auto cs_descriptor_pool = rs->context->create_descriptor_pool({
            .pool_sizes = {
                rhiDescriptorPoolSize{
                    .type = rhiDescriptorType::storage_image,
                    .count = cube_mip,
                },
                rhiDescriptorPoolSize{
                    .type = rhiDescriptorType::sampled_image,
                    .count = cube_mip + 1,
                },
                rhiDescriptorPoolSize{
                    .type = rhiDescriptorType::sampler,
                    .count = 1,
                },
                rhiDescriptorPoolSize{
                    .type = rhiDescriptorType::uniform_buffer,
                    .count = 1,
                },
            },
            .create_flags = rhiDescriptorPoolCreateFlags::update_after_bind
        }, 4);

    auto cs_pipeline_layout = rs->context->create_pipeline_layout({ cs_descriptor_layout1, cs_descriptor_layout2 }, sizeof(convertCB), nullptr);
    auto cs_descriptor_sets1 = rs->context->allocate_descriptor_sets(cs_descriptor_pool, { cs_descriptor_layout1 });
    auto cs_descriptor_sets2 = rs->context->allocate_descriptor_indexing_sets(cs_descriptor_pool, { cs_descriptor_layout2 }, { cube_mip });
    
    const rhiWriteDescriptor equirect_desc{
        .set = cs_descriptor_sets1[0],
        .binding = 0,
        .count = 1,
        .type = rhiDescriptorType::sampled_image,
        .image = { rhiDescriptorImageInfo{
            .sampler = nullptr,
            .texture = equirect_tex.get(),
            .mip = 0,
            .base_layer = 0,
            .layer_count = 1,
            .layout = rhiImageLayout::shader_readonly
        }}
    };
    const rhiDescriptorImageInfo sampler_image_info{
        .sampler = rs->samplers.linear_clamp.get()
    };
    const rhiWriteDescriptor sampler_write_desc{
        .set = cs_descriptor_sets1[0],
        .binding = 1,
        .array_index = 0,
        .count = 1,
        .type = rhiDescriptorType::sampler,
        .image = { sampler_image_info }
    };
    rs->context->update_descriptors({ equirect_desc, sampler_write_desc });

    // create compute pipeline
    auto cs = shaderio::load_shader_binary("E:\\Sponza\\Build\\shaders\\equirect_to_cube.cs.spv");
    const rhiComputePipelineDesc cs_desc{
        .cs = cs
    };
    auto cs_pipeline = rs->context->create_compute_pipeline(cs_desc, cs_pipeline_layout);

    // create cubemap
    const rhiTextureDesc d{
        .width = cube_resolution,
        .height = cube_resolution,
        .layers = cube_face_count,
        .mips = cube_mip,
        .format = rhiFormat::RGBA32_SFLOAT
    };
    sky_cubemap = draw_context->rs->context->create_texture_cubemap(d);

    // compute pipeline
    auto cmd = rs->context->begin_onetime_commands();
    
    std::vector<rhiDescriptorImageInfo> image_infos;
    image_infos.reserve(cube_mip);
    for (u32 mip = 0; mip < cube_mip; ++mip)
    {
        image_infos.push_back({
                .texture_cubemap = sky_cubemap.get(),
                .mip = mip,
                .base_layer = 0,
                .layer_count = cube_face_count,
                .layout = rhiImageLayout::general
            });
    }
    const rhiWriteDescriptor uav_desc{
        .set = cs_descriptor_sets2[0],
        .binding = 0,
        .array_index = 0,
        .count = cube_mip,
        .type = rhiDescriptorType::storage_image,
        .image = image_infos
    };
    rs->context->update_descriptors({ uav_desc });

    cmd->bind_pipeline(cs_pipeline.get());
    cmd->bind_descriptor_sets(cs_pipeline_layout, rhiPipelineType::compute, { cs_descriptor_sets1 }, 0, {});
    cmd->bind_descriptor_sets(cs_pipeline_layout, rhiPipelineType::compute, { cs_descriptor_sets2 }, 1, {});

    for (u32 mip = 0; mip < cube_mip; ++mip)
    {
        const u32 size = cube_resolution >> mip;
        cmd->image_barrier(sky_cubemap.get(), rhiImageLayout::undefined, rhiImageLayout::compute, mip, 1, 0, cube_face_count);
        
        const convertCB cb = { size, mip };
        cmd->push_constants(cs_pipeline_layout, rhiShaderStage::compute, 0, sizeof(convertCB), &cb);
        const u32 xy = (size + (dispatch_localgroupsize - 1)) / dispatch_localgroupsize;
        cmd->dispatch(xy, xy, cube_face_count);

        cmd->image_barrier(sky_cubemap.get(), rhiImageLayout::compute, rhiImageLayout::shader_readonly, mip, 1, 0, cube_face_count);
    }
    rs->context->submit_and_wait(cmd);
}

void skyPass::build_layouts(renderShared* rs)
{
    set_fragment_layout = rs->context->create_descriptor_set_layout(
        {
            rhiDescriptorSetLayoutBinding{
                .binding = 0,
                .type = rhiDescriptorType::uniform_buffer,
                .count = 1,
                .stage = rhiShaderStage::fragment
            },
            rhiDescriptorSetLayoutBinding{
                .binding = 1,
                .type = rhiDescriptorType::sampled_image,
                .count = 1,
                .stage = rhiShaderStage::fragment
            },
            rhiDescriptorSetLayoutBinding{
                .binding = 2,
                .type = rhiDescriptorType::sampler,
                .count = 1,
                .stage = rhiShaderStage::fragment
            },
        }
    );
    create_pipeline_layout(rs, { set_fragment_layout }, 0);
    create_descriptor_sets(rs, { set_fragment_layout });
}

void skyPass::build_attachments(rhiDeviceContext* context)
{
    render_info.samples = rhiSampleCount::x1;
    render_info.color_formats = { rhiFormat::RGBA8_UNORM };
    render_info.depth_format = std::nullopt;

    render_info.color_attachments.resize(1);
    render_info.color_attachments[0].load_op = rhiLoadOp::clear;
    render_info.color_attachments[0].store_op = rhiStoreOp::store;
    render_info.color_attachments[0].clear = { {0,0,0,1},1.0f,0 };

    const rhiTextureDesc td_color
    {
        .width = draw_context->w,
        .height = draw_context->h,
        .layers = 1,
        .mips = 1,
        .format = rhiFormat::RGBA8_UNORM,
        .samples = rhiSampleCount::x1,
        .is_depth = false
    };
    {
        const auto sky_context = static_cast<skyInitContext*>(draw_context.get());
        const auto scene_color = sky_context->scene_color.lock();
        rhiRenderTargetView rtview{
            .texture = scene_color.get(),
            .mip = 0,
            .layout = rhiImageLayout::color_attachment
        };
        render_info.color_attachments[0].view = rtview;
    }
}

void skyPass::build_pipeline(renderShared* rs)
{
    auto vs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\fullscreen.vs.spv");
    auto fs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\skybox.ps.spv");
    const rhiGraphicsPipelineDesc pipeline_desc{
        .vs = vs,
        .fs = fs,
        .color_formats = { rhiFormat::RGBA8_UNORM },
        .depth_format = std::nullopt,
        .samples = rhiSampleCount::x1,
        .depth_test = false,
        .depth_write = false
    };

    pipeline = rs->context->create_graphics_pipeline(pipeline_desc, pipeline_layout);

    shaderio::free_shader_binary(vs);
    shaderio::free_shader_binary(fs);
}

void skyPass::update(drawUpdateContext* update_context)
{
    skyUpdateContext* context = static_cast<skyUpdateContext*>(update_context);
    assert(context);

    const rhiDescriptorBufferInfo buffer_info{
        .buffer = context->global_buf,
        .offset = 0,
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

    const rhiWriteDescriptor cube_desc{
        .set = descriptor_sets[image_index.value()][0],
        .binding = 1,
        .array_index = 0,
        .count = 1,
        .type = rhiDescriptorType::sampled_image,
        .image = { rhiDescriptorImageInfo{
                .texture_cubemap = sky_cubemap.get(),
                .mip = 0,
                .base_layer = 0,
                .layer_count = 1,
                .cubemap_viewtype = rhiCubemapViewType::cube,
                .layout = rhiImageLayout::shader_readonly
            } }
    };

    const rhiDescriptorImageInfo cube_sampler_image_info{
        .sampler = draw_context->rs->samplers.linear_clamp.get()
    };
    const rhiWriteDescriptor cube_sampler_write_desc{
        .set = descriptor_sets[image_index.value()][0],
        .binding = 2,
        .array_index = 0,
        .count = 1,
        .type = rhiDescriptorType::sampler,
        .image = { cube_sampler_image_info }
    };
    draw_context->rs->context->update_descriptors({ write_desc, cube_desc, cube_sampler_write_desc });
}

void skyPass::draw(rhiCommandList* cmd)
{
    cmd->draw_fullscreen();
}

void skyPass::begin_barrier(rhiCommandList* cmd)
{
    const auto sky_context = static_cast<skyInitContext*>(draw_context.get());
    const auto scene_color = sky_context->scene_color.lock();
    if (is_first_frame)
    {
        cmd->image_barrier(scene_color.get(), rhiImageLayout::undefined, rhiImageLayout::color_attachment);
        is_first_frame = false;
    }
    else
        cmd->image_barrier(scene_color.get(), rhiImageLayout::shader_readonly, rhiImageLayout::color_attachment);
}

void skyPass::end_barrier(rhiCommandList* cmd)
{
}