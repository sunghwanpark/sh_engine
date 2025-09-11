#pragma once
#include "pch.h"
#include "rhi/rhiBindlessTable.h"
#include "vkDeviceContext.h"

class vkBindlessTable final : public rhiBindlessTable 
{
public:
    vkBindlessTable(vkDeviceContext* context, const rhiBindlessDesc& desc, u32 set_index);
    ~vkBindlessTable() override;

    void create_sampled_image(rhiTexture* tex, u32 base_mip = 0) override;
    void create_sampler(rhiSampler* sampler) override;

    void update_sampled_image(rhiBindlessHandle h, rhiTexture* tex, u32 base_mip = 0) override;
    void bind_once(rhiCommandList* cmd, rhiPipelineLayout layout, u32 set_index) override;
    rhiDescriptorSetLayout get_set_layout() override;

private:
    VkDevice device;
    rhiBindlessDesc  desc;
    u32 set_index;

    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
};
