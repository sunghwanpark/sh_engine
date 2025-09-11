#include "vkPipeline.h"
#include "vkCommon.h"

vkPipelineLayoutHolder::~vkPipelineLayoutHolder()
{
	vkDestroyPipelineLayout(device, layout, nullptr);
}

vkPipeline::vkPipeline(VkDevice device, const rhiGraphicsPipelineDesc& desc, rhiPipelineType t)
	: rhiPipeline(desc, t), device(device), bind_point(vk_bind_point(t))
{
}

vkPipeline::vkPipeline(VkDevice device, const rhiComputePipelineDesc& desc, rhiPipelineType t)
	: rhiPipeline(desc, t), device(device), bind_point(vk_bind_point(t))
{
}

vkPipeline::~vkPipeline()
{
	vkDestroyPipeline(device, handle, nullptr);
}
