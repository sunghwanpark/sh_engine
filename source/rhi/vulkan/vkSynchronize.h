#pragma once

#include "pch.h"
#include "rhi/rhiSynchroize.h"

class vkSemaphore final : public rhiSemaphore
{
public:
	vkSemaphore(VkDevice device);
	virtual ~vkSemaphore();

	VkSemaphore handle() { return semaphore; }

private:
	VkDevice device;
	VkSemaphore semaphore;
};

class vkFence final : public rhiFence 
{
public:
	vkFence(VkDevice device, bool signaled);
	virtual ~vkFence();
	
	VkFence handle() { return fence; }

private:
	VkDevice device;
	VkFence  fence = VK_NULL_HANDLE;
};