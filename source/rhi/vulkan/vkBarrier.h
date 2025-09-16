#pragma once

#include "pch.h"
#include "rhi/rhiDefs.h"

struct vkStageAccess
{
	VkPipelineStageFlags2 stage;
	VkAccessFlags2 access;
};

vkStageAccess vk_stage_access(rhiImageLayout layout)
{
    switch (layout)
    {
    case rhiImageLayout::color_attachment:
        return { VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                 VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT };
    case rhiImageLayout::shader_readonly:
        return { VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                 VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT };
    case rhiImageLayout::depth_stencil_attachment:
        return { VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT };
    case rhiImageLayout::depth_readonly:
        return { VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT };
    case rhiImageLayout::transfer_src:
        return { VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT };
    case rhiImageLayout::transfer_dst:
        return { VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT };
    case rhiImageLayout::present:
        return { VK_PIPELINE_STAGE_2_NONE, 0 };
    case rhiImageLayout::compute:
        return { VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT };
    case rhiImageLayout::general:
        return { VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT };
    case rhiImageLayout::undefined:
        return { VK_PIPELINE_STAGE_2_NONE, 0 };
    }
    return { VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0 };
}
