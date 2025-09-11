#pragma once

#include "pch.h"
#include "rhi/rhiGraphicsQueue.h"

class vkDeviceContext;
class vkGraphicsQueue final : public rhiGraphicsQueue
{
public:
	vkGraphicsQueue(VkDevice device, VkQueue queue, const u32 f_index)
		: device(device), queue(queue), family_index(f_index)
	{}
public:
	void submit(const rhiSubmitInfo& info) override;

private:
	VkDevice device = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;
	u32 family_index = 0;
};