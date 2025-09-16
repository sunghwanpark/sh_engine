#include "renderShared.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiFrameContext.h"
#include "rhi/rhiPipeline.h"
#include "rhi/rhiDescriptor.h"
#include "rhi/rhiTextureView.h"
#include "rhi/rhiCommandList.h"
#include "rhi/rhiSampler.h"
#include "rhi/rhiBuffer.h"
#include "rhi/rhiSwapChain.h"

renderShared::~renderShared()
{
    samplers.linear_clamp.reset();
    samplers.linear_wrap.reset();
    samplers.point_clamp.reset();
    scene_color.reset();
}

void renderShared::Initialize(rhiDeviceContext* context, rhiFrameContext* frame_context)
{
    this->context = context;
    this->frame_context = frame_context;
    create_shared_samplers();
    create_descriptor_pools();
    create_scene_color();
}

void renderShared::create_or_resize_buffer(std::shared_ptr<rhiBuffer>& buffer, const u32 bytes, const rhiBufferUsage usage, const rhiMem mem, const u32 stride)
{
    if (bytes == 0)
    {
        buffer.reset();
        return;
    }
    if (buffer && buffer->size() >= bytes)
        return;

    auto buf = context->create_buffer(rhiBufferDesc
        {
            .size = bytes,
            .usage = usage,
            .memory = mem,
            .stride = stride
        });
    buffer = std::move(buf);
}

void renderShared::create_or_resize_buffer(std::unique_ptr<rhiBuffer>& buffer, const u32 bytes, const rhiBufferUsage usage, const rhiMem mem, const u32 stride)
{
    if (bytes == 0)
    {
        buffer.reset();
        return;
    }
    if (buffer && buffer->size() >= bytes)
        return;

    buffer = context->create_buffer(rhiBufferDesc
        {
            .size = bytes,
            .usage = usage,
            .memory = mem,
            .stride = stride
        });
}

void renderShared::upload_to_device(rhiCommandList* cmd_list, rhiBuffer* buffer, const void* src, const u32 bytes, const u32 dst_offset)
{
    assert(dst_offset + bytes <= buffer->size());
    assert((dst_offset % 4) == 0 && (bytes % 4) == 0);

    auto staging_buffer = context->create_buffer(rhiBufferDesc
        {
            .size = bytes,
            .usage = rhiBufferUsage::transfer_src,
            .memory = rhiMem::auto_host
        });

    void* p = staging_buffer->map();
    std::memcpy(p, src, bytes);
    staging_buffer->flush(0, bytes);
    staging_buffer->unmap();

    cmd_list->copy_buffer(staging_buffer.get(), 0, buffer, dst_offset, bytes);
    pending_staging_buffers.push_back(std::move(staging_buffer));
}

void renderShared::upload_to_device(rhiBuffer* buffer, const void* src, const u32 bytes, const u32 dst_offset)
{
    assert(dst_offset + bytes <= buffer->size());
    assert((dst_offset % 4) == 0 && (bytes % 4) == 0);

    auto staging_buffer = context->create_buffer(rhiBufferDesc
        {
            .size = bytes,
            .usage = rhiBufferUsage::transfer_src,
            .memory = rhiMem::auto_host
        });

    void* p = staging_buffer->map();
    std::memcpy(p, src, bytes);
    staging_buffer->flush(0, bytes);
    staging_buffer->unmap();

    auto cmd_list = context->begin_onetime_commands();
    cmd_list->copy_buffer(staging_buffer.get(), 0, buffer, dst_offset, bytes);
    context->submit_and_wait(cmd_list);
}

const u32 renderShared::get_frame_size() const
{
    return frame_context->get_frame_size();
}

void renderShared::create_shared_samplers()
{
    samplers.linear_clamp = context->create_sampler({
        .mag_filter = rhiFilter::linear,
        .min_filter = rhiFilter::linear,
        .mipmap_mode = rhiMipmapMode::linear,
        .address_u = rhiAddressMode::clamp_to_edge,
        .address_v = rhiAddressMode::clamp_to_edge,
        .address_w = rhiAddressMode::clamp_to_edge
        });

    samplers.linear_wrap = context->create_sampler({
        .mag_filter = rhiFilter::linear,
        .min_filter = rhiFilter::linear,
        .mipmap_mode = rhiMipmapMode::linear,
        .address_u = rhiAddressMode::repeat,
        .address_v = rhiAddressMode::repeat,
        .address_w = rhiAddressMode::repeat
        });

    samplers.point_clamp = context->create_sampler({
        .mag_filter = rhiFilter::nearest,
        .min_filter = rhiFilter::nearest,
        .mipmap_mode = rhiMipmapMode::nearest,
        .address_u = rhiAddressMode::clamp_to_edge,
        .address_v = rhiAddressMode::clamp_to_edge,
        .address_w = rhiAddressMode::clamp_to_edge
        });
}

void renderShared::create_scene_color()
{
    const u32 width = frame_context->swapchain->width();
    const u32 height = frame_context->swapchain->height();
    auto tex = context->create_texture(rhiTextureDesc{
        .width = width,
        .height = height,
        .layers = 1,
        .mips = 1,
        .format = rhiFormat::RGBA8_UNORM,
        .samples = rhiSampleCount::x1,
        .is_depth = false
        });

    scene_color.reset(tex.release());
}

void renderShared::create_descriptor_pools()
{
    arena.pools.resize(frame_context->swapchain->get_swapchain_image_count());
    for (u32 i = 0; i < frame_context->swapchain->get_swapchain_image_count(); i++)
    {
        arena.pools[i] = context->create_descriptor_pool({
                .pool_sizes = {
                    {
                        .type = rhiDescriptorType::uniform_buffer,
                        .count = 256
                    },
                    {
                        .type = rhiDescriptorType::sampled_image,
                        .count = 1024
                    },
                    {
                        .type = rhiDescriptorType::sampler,
                        .count = 64
                    },
                    {
                        .type = rhiDescriptorType::storage_buffer,
                        .count = 256
                    } 
                }
            }, 1024);
    }
}

void renderShared::clear_staging_buffer()
{
    pending_staging_buffers.clear();
}