#include "vkSynchronize.h"
#include "vkCommon.h"

vkSemaphore::vkSemaphore(VkDevice device)
	: device(device)
{
	VkSemaphoreCreateInfo create_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VK_CHECK_ERROR(vkCreateSemaphore(device, &create_info, nullptr, &semaphore));
}

vkSemaphore::~vkSemaphore()
{
	vkDestroySemaphore(device, semaphore, nullptr);
}

vkFence::vkFence(VkDevice device, bool signaled)
	: device(device)
{
	VkFenceCreateInfo create_info{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	if (signaled)
		create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	VK_CHECK_ERROR(vkCreateFence(device, &create_info, nullptr, &fence));
}

vkFence::~vkFence()
{
	vkDestroyFence(device, fence, nullptr);
}
