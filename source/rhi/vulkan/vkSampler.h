#pragma once

#include "pch.h"
#include "rhi/rhiSampler.h"

class vkDeviceContext;
class vkSampler final : public rhiSampler
{
public:
	vkSampler(vkDeviceContext* context, const rhiSamplerDesc& desc);
	virtual ~vkSampler();

public:
	VkSampler get_sampler() const { return sampler; }

private:
	VkDevice device;
	VkSampler sampler = VK_NULL_HANDLE;
};