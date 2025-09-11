#pragma once

#include "pch.h"
#include "rhi/rhiFrameContext.h"

class vkDeviceContext;
struct vkFrameContext final : rhiFrameContext
{
public:
    void update_inflight(std::weak_ptr<vkDeviceContext> context, const u32 frame_count, const u32 gfx_queue_findex);
    void create_graphics_queue(VkDevice device, VkQueue gfx_queue, const u32 gfx_queue_findex);

private:
    VkCommandPool create_pool(VkDevice device, u32 family, VkCommandPoolCreateFlags flags);
    VkCommandBuffer alloc_primary(VkDevice device, VkCommandPool pool);
};