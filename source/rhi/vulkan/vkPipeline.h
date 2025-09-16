#pragma once

#include "pch.h"
#include "rhi/rhiPipeline.h"

struct vkPipelineLayoutHolder 
{
    vkPipelineLayoutHolder(VkDevice device) : device(device) {}
    ~vkPipelineLayoutHolder();

    VkDevice device;
    VkPipelineLayout layout = VK_NULL_HANDLE;
};

class vkPipeline final : public rhiPipeline 
{
public:
    vkPipeline(VkDevice device, const rhiGraphicsPipelineDesc& desc, rhiPipelineType t);
    vkPipeline(VkDevice device, const rhiComputePipelineDesc& desc, rhiPipelineType t);
    virtual ~vkPipeline();

    VkDevice  device;
    VkPipeline handle = VK_NULL_HANDLE;
    VkPipelineBindPoint bind_point;
};