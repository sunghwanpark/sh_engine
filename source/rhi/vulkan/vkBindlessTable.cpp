#include "vkBindlessTable.h"
#include "vkCommon.h"
#include "vkTexture.h"
#include "vkSampler.h"
#include "vkCommandList.h"

vkBindlessTable::vkBindlessTable(vkDeviceContext* context, const rhiBindlessDesc& desc, u32 set_index)
    : device(context->device), desc(desc), set_index(set_index)
{
    // descriptor set layout
    const VkDescriptorSetLayoutBinding sampler{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .descriptorCount = desc.max_samplers,
        .stageFlags = VK_SHADER_STAGE_ALL
    };

    const VkDescriptorSetLayoutBinding tex{
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = desc.max_sampled_images,
        .stageFlags = VK_SHADER_STAGE_ALL
    };
    const std::array<VkDescriptorSetLayoutBinding, 2> bindings = { sampler, tex };
    const std::array<VkDescriptorBindingFlags, 2> flags = {
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
    };
    const VkDescriptorSetLayoutBindingFlagsCreateInfo flags_createinfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 2,
        .pBindingFlags = flags.data()
    };
    const VkDescriptorSetLayoutCreateInfo layout_createinfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &flags_createinfo,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = 2,
        .pBindings = bindings.data(),
    };
    VK_CHECK_ERROR(vkCreateDescriptorSetLayout(device, &layout_createinfo, nullptr, &set_layout));

    // Descriptor pool
    const std::array<VkDescriptorPoolSize, 2> pool_sizes = {
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = desc.max_samplers
        },
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 
            .descriptorCount = desc.max_sampled_images
        }
    };
    const VkDescriptorPoolCreateInfo pool_createinfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = 1,
        .poolSizeCount = 2,
        .pPoolSizes = pool_sizes.data()
    };
    VK_CHECK_ERROR(vkCreateDescriptorPool(device, &pool_createinfo, nullptr, &pool));

    // Allocate descriptor set
    const std::array<u32, 1> counts = { desc.max_sampled_images };
    const VkDescriptorSetVariableDescriptorCountAllocateInfo count_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = counts.data()
    };
    const VkDescriptorSetAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &count_info,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &set_layout
    };
    VK_CHECK_ERROR(vkAllocateDescriptorSets(device, &allocate_info, &set));
}

vkBindlessTable::~vkBindlessTable() 
{
    if (pool) 
        vkDestroyDescriptorPool(device, pool, nullptr);
    if (set_layout)
        vkDestroyDescriptorSetLayout(device, set_layout, nullptr);
}

void vkBindlessTable::create_sampled_image(rhiTexture* tex, u32 base_mip)
{
    auto vk_tex = static_cast<vkTexture*>(tex);
    const VkDescriptorImageInfo info{
        .imageView = vk_tex->get_view(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = 1,
        .dstArrayElement = next_image,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &info
    };
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void vkBindlessTable::create_sampler(rhiSampler* sampler)
{
    auto vk_sampler = static_cast<vkSampler*>(sampler);
    const VkDescriptorImageInfo info{
        .sampler = vk_sampler->get_sampler()
    };
    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = 0,
        .dstArrayElement = next_sampler,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &info
    };
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void vkBindlessTable::update_sampled_image(rhiBindlessHandle h, rhiTexture* tex, u32 base_mip)
{
    auto vk_tex = static_cast<vkTexture*>(tex);
    const VkDescriptorImageInfo info{
        .imageView = vk_tex->get_view(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = 1,
        .dstArrayElement = h.index,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &info
    };
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void vkBindlessTable::bind_once(rhiCommandList* cmd, rhiPipelineLayout layout, u32 set_index) 
{
    auto vk_cmd = static_cast<vkCommandList*>(cmd);
    vkCmdBindDescriptorSets(vk_cmd->get_cmd_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, get_vk_pipeline_layout(layout), set_index, 1, &set, 0, nullptr);
}

rhiDescriptorSetLayout vkBindlessTable::get_set_layout()
{
    return rhiDescriptorSetLayout{ .native = set_layout };
}