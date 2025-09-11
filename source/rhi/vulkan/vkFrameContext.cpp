#include "vkFrameContext.h"
#include "vkDeviceContext.h"
#include "vkCommon.h"
#include "vkCommandList.h"
#include "vkSynchronize.h"
#include "vkGraphicsQueue.h"

void vkFrameContext::update_inflight(std::weak_ptr<vkDeviceContext> context, const u32 frame_count, const u32 gfx_queue_findex)
{
	auto ptr = context.lock();
	frames.resize(frame_count);
	for (u32 index = 0; index < frame_count; ++index)
	{
		VkCommandPool pool = create_pool(ptr->device, gfx_queue_findex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
		VkCommandBuffer buf = alloc_primary(ptr->device, pool);
		frames[index].cmd = std::make_unique<vkCommandList>(ptr.get(), pool, buf, false);
		frames[index].image_available = std::make_unique<vkSemaphore>(ptr->device);
		frames[index].render_finished = std::make_unique<vkSemaphore>(ptr->device);
		frames[index].in_flight = std::make_unique<vkFence>(ptr->device, true);
	}
}

void vkFrameContext::create_graphics_queue(VkDevice device, VkQueue gfx_queue, const u32 gfx_queue_findex)
{
	graphics_queue = std::make_unique<vkGraphicsQueue>(device, gfx_queue, gfx_queue_findex);
}

VkCommandPool vkFrameContext::create_pool(VkDevice device, u32 family, VkCommandPoolCreateFlags flags)
{
	VkCommandPoolCreateInfo create_info
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = flags,
		.queueFamilyIndex = family
	};
	VkCommandPool pool;
	VK_CHECK_ERROR(vkCreateCommandPool(device, &create_info, nullptr, &pool));
	return pool;
}

VkCommandBuffer vkFrameContext::alloc_primary(VkDevice device, VkCommandPool pool)
{
	VkCommandBufferAllocateInfo allocate_info
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	VkCommandBuffer buffer;
	VK_CHECK_ERROR(vkAllocateCommandBuffers(device, &allocate_info, &buffer));
	return buffer;
}