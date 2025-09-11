#include "vkCommandList.h"
#include "vkCommon.h"
#include "vkDeviceContext.h"
#include "vkImageViewCache.h"
#include "vkPipeline.h"
#include "vkBarrier.h"
#include "rhi/rhiPipeline.h"
#include "rhi/rhiRenderpass.h"
#include "rhi/rhiBuffer.h"
#include "vkBuffer.h"

vkCommandList::vkCommandList(vkDeviceContext* context, VkCommandPool pool, VkCommandBuffer cmd_buffer, bool is_transient)
    : device(context->device),
    phys_device(context->phys_device),
    weak_imgview_cache(context->get_imageview_cache()),
    cmd_pool(pool),
    cmd_buffer(cmd_buffer),
    is_transient(is_transient) {}

vkCommandList::~vkCommandList()
{
    vkDestroyCommandPool(device, cmd_pool, nullptr);
}

void vkCommandList::allocate()
{
	if (cmd_buffer != VK_NULL_HANDLE)
		return;

	const VkCommandBufferAllocateInfo allocate_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = cmd_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	VK_CHECK_ERROR(vkAllocateCommandBuffers(device, &allocate_info, &cmd_buffer));
}

void vkCommandList::begin(u32 flags)
{
    assert(cmd_buffer != VK_NULL_HANDLE);
	VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	if (flags & 0x1u)
		begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK_ERROR(vkBeginCommandBuffer(cmd_buffer, &begin_info));
}

void vkCommandList::end()
{
    assert(cmd_buffer != VK_NULL_HANDLE);
	VK_CHECK_ERROR(vkEndCommandBuffer(cmd_buffer));
}

void vkCommandList::reset()
{
    assert(cmd_buffer != VK_NULL_HANDLE);
	VK_CHECK_ERROR(vkResetCommandBuffer(cmd_buffer, 0));
}

void vkCommandList::begin_render_pass(const rhiRenderingInfo& info)
{
    assert(cmd_buffer != VK_NULL_HANDLE);

	auto ptr = weak_imgview_cache.lock();
    std::vector<VkRenderingAttachmentInfo> color_attachments;
    color_attachments.reserve(info.color_attachments.size());
	for (const auto& color_info : info.color_attachments)
	{
		const VkClearColorValue clear_value{
			.float32 = { color_info.clear.rgba[0], color_info.clear.rgba[1], color_info.clear.rgba[2], color_info.clear.rgba[3] }
		};

        VkRenderingAttachmentInfo color_attchment_info{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        if (color_info.rt_desc.has_value())
            color_attchment_info.imageView = ptr->get(&color_info.rt_desc.value());
        else
            color_attchment_info.imageView = ptr->get_or_create(&color_info.view.value());
         
        color_attchment_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attchment_info.loadOp = vk_load_op(color_info.load_op);
        color_attchment_info.storeOp = vk_store_op(color_info.store_op);
        color_attchment_info.clearValue = VkClearValue{ .color = clear_value };
        color_attachments.push_back(color_attchment_info);
	}

    VkRenderingAttachmentInfo depth_attachment_info{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    VkRenderingAttachmentInfo stencil_attachment_info{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    if (info.depth_attachment.has_value())
    {
        const auto& depth_attachment = info.depth_attachment.value();
        depth_attachment_info.imageView = ptr->get_or_create(&depth_attachment.view.value());
        depth_attachment_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth_attachment_info.loadOp = vk_load_op(depth_attachment.load_op);
        depth_attachment_info.storeOp = vk_store_op(depth_attachment.store_op);
        depth_attachment_info.clearValue = {
            .depthStencil = {
                .depth = depth_attachment.clear.depth
            }
        };
    }
    if (info.stencil_attachment.has_value())
    {
        const auto& stencil_attachment = info.stencil_attachment.value();
        stencil_attachment_info.imageView = ptr->get_or_create(&stencil_attachment.view.value());
        stencil_attachment_info.imageLayout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
        stencil_attachment_info.loadOp = vk_load_op(stencil_attachment.load_op);
        stencil_attachment_info.storeOp = vk_store_op(stencil_attachment.store_op);
        stencil_attachment_info.clearValue = {
            .depthStencil = {
                .stencil = stencil_attachment.clear.stencil
            }
        };
    }

    const VkRenderingInfo render_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = VkRect2D{
            .offset = VkOffset2D{
                .x = static_cast<i32>(info.render_area.x),
                .y = static_cast<i32>(info.render_area.y)
            },
            .extent = VkExtent2D{
                .width = static_cast<u32>(info.render_area.z),
                .height = static_cast<u32>(info.render_area.w)
            },
        },
        .layerCount = info.layer_count,
        .colorAttachmentCount = static_cast<u32>(color_attachments.size()),
        .pColorAttachments = color_attachments.size() > 0 ? color_attachments.data() : nullptr,
        .pDepthAttachment = info.depth_attachment.has_value() ? &depth_attachment_info : nullptr,
        .pStencilAttachment = info.stencil_attachment.has_value() ? &stencil_attachment_info : nullptr
    };
    vkCmdBeginRendering(cmd_buffer, &render_info);
}

void vkCommandList::end_render_pass()
{
    assert(cmd_buffer != VK_NULL_HANDLE);
    vkCmdEndRendering(cmd_buffer);
}

void vkCommandList::set_viewport_scissor(const vec2 vp_size)
{
    assert(cmd_buffer != VK_NULL_HANDLE);
    const VkViewport vp{
        .x = 0.f,
        .y = 0.f,
        .width = vp_size.x,
        .height = vp_size.y,
        .minDepth = 0.f,
        .maxDepth = 1.f
    };
    vkCmdSetViewport(cmd_buffer, 0, 1, &vp);

    const VkRect2D sc{
        .offset = {0, 0},
        .extent = { static_cast<u32>(vp_size.x), static_cast<u32>(vp_size.y)}
    };
    vkCmdSetScissor(cmd_buffer, 0, 1, &sc);
}

void vkCommandList::copy_buffer(rhiBuffer* src, const u32 src_offset, rhiBuffer* dst, const u32 dst_offset, const u64 bytes)
{
    assert(cmd_buffer != VK_NULL_HANDLE);
    VkBuffer vk_src = reinterpret_cast<VkBuffer>(src->native());
    VkBuffer vk_dst = reinterpret_cast<VkBuffer>(dst->native());

    const VkBufferCopy copy{
        .srcOffset = src_offset,
        .dstOffset = dst_offset,
        .size = bytes
    };
    vkCmdCopyBuffer(cmd_buffer, vk_src, vk_dst, 1, &copy);
}

void vkCommandList::copy_buffer_to_image(rhiBuffer* src_buf, rhiTexture* dst_tex, rhiImageLayout layout, std::span<const rhiBufferImageCopy> regions)
{
    assert(cmd_buffer != VK_NULL_HANDLE);
    auto vk_src = static_cast<vkBuffer*>(src_buf);
    auto vk_dst = static_cast<vkTexture*>(dst_tex);

    std::vector<VkBufferImageCopy> vk_regions;
    vk_regions.reserve(regions.size());
    for (const auto& r : regions) 
    {
        VkBufferImageCopy c{
            .bufferOffset = r.buffer_offset,
            .bufferRowLength = r.buffer_rowlength,
            .bufferImageHeight = r.buffer_imageheight,
            .imageSubresource = VkImageSubresourceLayers{
                .aspectMask = vk_aspect(r.imageSubresource.aspect),
                .mipLevel = r.imageSubresource.mip_level,
                .baseArrayLayer = r.imageSubresource.base_array_layer,
                .layerCount = r.imageSubresource.layer_count
            },
            .imageOffset = { r.image_offset.x, r.image_offset.y, r.image_offset.z },
            .imageExtent = { r.image_extent.x, r.image_extent.y, r.image_extent.z }
        };
        vk_regions.push_back(c);
    }

    vkCmdCopyBufferToImage(cmd_buffer, vk_src->handle(), vk_dst->get_image(), vk_layout(layout), static_cast<u32>(vk_regions.size()), vk_regions.data());
}

void vkCommandList::generate_mips(rhiTexture* tex, const rhiGenMipsDesc& desc)
{
    auto vk_tex = static_cast<vkTexture*>(tex);
    const VkFormat fmt = vk_tex->get_format();
    const VkImage img = vk_tex->get_image();
    const u32 mip_count = desc.mip_count ? desc.mip_count : tex->desc.mips - desc.base_mip;
    const u32 layer_count = desc.layer_count ? desc.layer_count : tex->desc.layers;

    bool use_linear = (desc.method != rhiMipsMethod::compute);
    if (use_linear)
    {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(phys_device, fmt, &props);
        if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) 
        {
            use_linear = false;
        }
    }

    if (!use_linear) 
    {
        generate_mips_compute(tex, desc);
        return;
    }

    for (u32 layer = 0; layer < layer_count; ++layer) 
    {
        u32 mip_width = std::max(1u, tex->desc.width >> desc.base_mip);
        u32 mip_height = std::max(1u, tex->desc.height >> desc.base_mip);

        for (u32 i = 1; i < mip_count; ++i) 
        {
            const u32 src_mip = desc.base_mip + i - 1;
            const u32 dst_mip = desc.base_mip + i;
            const u32 arr = desc.base_layer + layer;

            image_barrier(tex, rhiImageLayout::undefined, rhiImageLayout::transfer_dst, dst_mip, 1, arr, 1);

            const VkExtent3D dst_extent{std::max(1u, mip_width >> 1), std::max(1u, mip_height >> 1), 1};
            const VkImageBlit blit{
                .srcSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = src_mip,
                    .baseArrayLayer = arr,
                    .layerCount = 1
                },
                .srcOffsets = {
                    { 0, 0, 0 },
                    { static_cast<i32>(mip_width), static_cast<i32>(mip_height), 1 }
                },
                .dstSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = dst_mip,
                    .baseArrayLayer = arr,
                    .layerCount = 1,
                },
                .dstOffsets = {
                    { 0, 0, 0 },
                    { static_cast<i32>(dst_extent.width), static_cast<i32>(dst_extent.height), 1 }
                }
            };

            vkCmdBlitImage(cmd_buffer, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

            image_barrier(tex, rhiImageLayout::transfer_dst, rhiImageLayout::transfer_src, dst_mip, 1, arr, 1);
            mip_width = dst_extent.width;
            mip_height = dst_extent.height;
        }
    }

    image_barrier(tex, rhiImageLayout::transfer_src, rhiImageLayout::shader_readonly, desc.base_mip, mip_count, desc.base_layer, layer_count);
}

void vkCommandList::generate_mips_compute(rhiTexture* tex, const rhiGenMipsDesc& desc)
{
    // todo
}

void vkCommandList::image_barrier(rhiTexture* tex, rhiImageLayout old_layout, rhiImageLayout new_layout, u32 base_mip, u32 level_count, u32 base_layer, u32 layer_count, bool is_same_stage)
{
    assert(cmd_buffer != VK_NULL_HANDLE);
    auto vk_tex = static_cast<vkTexture*>(tex);

    if (level_count == 0) 
        level_count = tex->desc.mips;
    if (layer_count == 0) 
        layer_count = tex->desc.layers;

    auto src = vk_stage_access(old_layout);
    auto dst = vk_stage_access(new_layout);
    if (is_same_stage)
    {
        src.stage = dst.stage;
    }

    const VkImageMemoryBarrier2 barrier{ 
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = src.stage,
        .srcAccessMask = src.access,
        .dstStageMask = dst.stage,
        .dstAccessMask = dst.access,
        .oldLayout = vk_layout(old_layout),
        .newLayout = vk_layout(new_layout),
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vk_tex->get_image(),
        .subresourceRange = {
            .aspectMask = vk_aspect_from_format(vk_tex->desc.format),
            .baseMipLevel = base_mip,
            .levelCount = level_count,
            .baseArrayLayer = base_layer,
            .layerCount = layer_count
        }
    };

    const VkDependencyInfo dependency{ 
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    vkCmdPipelineBarrier2(cmd_buffer, &dependency);
}

void vkCommandList::buffer_barrier(rhiBuffer* buf, rhiPipelineStage src_stage, rhiPipelineStage dst_stage, rhiAccessFlags src_access, rhiAccessFlags dst_access, u32 offset, u64 size)
{
    assert(cmd_buffer != VK_NULL_HANDLE);
    assert(buf);

    VkBuffer vk_buf = reinterpret_cast<VkBuffer>(buf->native());

    const VkBufferMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = vk_pipeline_stage2(src_stage),
        .srcAccessMask = vk_access_flags2(src_access),
        .dstStageMask = vk_pipeline_stage2(dst_stage),
        .dstAccessMask = vk_access_flags2(dst_access),
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = vk_buf,
        .offset = static_cast<VkDeviceSize>(offset),
        .size = static_cast<VkDeviceSize>(size),
    };

    const VkDependencyInfo dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &barrier
    };
    vkCmdPipelineBarrier2(cmd_buffer, &dependency);
}

void vkCommandList::bind_pipeline(rhiPipeline* p)
{
    assert(cmd_buffer != VK_NULL_HANDLE);
    auto vk_pipeline = static_cast<vkPipeline*>(p);
    assert(vk_pipeline);
    vkCmdBindPipeline(cmd_buffer, vk_pipeline->bind_point, vk_pipeline->handle);
}

void vkCommandList::bind_vertex_buffer(rhiBuffer* vbo, const u32 slot, const u32 offset)
{
    assert(cmd_buffer != VK_NULL_HANDLE);
    VkBuffer buf = reinterpret_cast<VkBuffer>(vbo->native());
    const VkDeviceSize size = offset;
    vkCmdBindVertexBuffers(cmd_buffer, slot, 1, &buf, &size);
}

void vkCommandList::bind_index_buffer(rhiBuffer* ibo, const u32 offset)
{
    assert(cmd_buffer != VK_NULL_HANDLE);
    VkBuffer buf = reinterpret_cast<VkBuffer>(ibo->native());
    vkCmdBindIndexBuffer(cmd_buffer, buf, offset, VK_INDEX_TYPE_UINT32);
}

void vkCommandList::bind_descriptor_sets(rhiPipelineLayout layout, rhiPipelineType pipeline, const std::vector<rhiDescriptorSet>& sets, const u32 first_set, const std::vector<u32>& dynamic_offsets)
{
    assert(cmd_buffer != VK_NULL_HANDLE);
    
    std::vector<VkDescriptorSet> vk_descriptor_sets;
    vk_descriptor_sets.reserve(sets.size());
    for (const rhiDescriptorSet& s : sets)
    {
        vk_descriptor_sets.push_back(get_vk_descriptor_set(s));
    }

    vkCmdBindDescriptorSets(cmd_buffer, vk_bind_point(pipeline), get_vk_pipeline_layout(layout), first_set, static_cast<u32>(sets.size()), vk_descriptor_sets.data(), static_cast<u32>(dynamic_offsets.size()), dynamic_offsets.empty() ? nullptr : dynamic_offsets.data());
}

void vkCommandList::push_constants(const rhiPipelineLayout layout, rhiShaderStage stages, u32 offset, u32 size, const void* data)
{
    VkShaderStageFlags stage{};
    if (static_cast<u32>(stages) & static_cast<u32>(rhiShaderStage::all))
    {
        stage = VK_SHADER_STAGE_ALL;
    }
    else
    {
        if (static_cast<u32>(stages) & static_cast<u32>(rhiShaderStage::vertex))
            stage |= VK_SHADER_STAGE_VERTEX_BIT;
        if (static_cast<u32>(stages) & static_cast<u32>(rhiShaderStage::fragment))
            stage |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if (static_cast<u32>(stages) & static_cast<u32>(rhiShaderStage::compute))
            stage |= VK_SHADER_STAGE_COMPUTE_BIT;
    }

    vkCmdPushConstants(cmd_buffer, get_vk_pipeline_layout(layout), stage, offset, size, data);
}

void vkCommandList::draw_indexed_indirect(rhiBuffer* indirect_buffer, const u32 offset, const u32 draw_count, const u32 stride)
{
    assert(cmd_buffer != VK_NULL_HANDLE);
    VkBuffer buf = reinterpret_cast<VkBuffer>(indirect_buffer->native());
    vkCmdDrawIndexedIndirect(cmd_buffer, buf, VkDeviceSize(offset), draw_count, stride);
}

void vkCommandList::draw_fullscreen()
{
    assert(cmd_buffer != VK_NULL_HANDLE);
    vkCmdDraw(cmd_buffer, 3, 1, 0, 0);
}

void vkCommandList::dispatch(const u32 x, const u32 y, const u32 z)
{
    assert(cmd_buffer != VK_NULL_HANDLE);
    vkCmdDispatch(cmd_buffer, x, y, z);
}
