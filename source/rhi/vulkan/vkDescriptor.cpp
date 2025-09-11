#include "vkDescriptor.h"

vkDescriptorSetLayoutHolder::vkDescriptorSetLayoutHolder(VkDevice device)
    : device(device)
{
}

vkDescriptorSetLayoutHolder::~vkDescriptorSetLayoutHolder()
{
    vkDestroyDescriptorSetLayout(device, layout, nullptr);
}

vkDescriptorPoolHolder::vkDescriptorPoolHolder(VkDevice device)
    : device(device)
{
}

vkDescriptorPoolHolder::~vkDescriptorPoolHolder()
{
    vkDestroyDescriptorPool(device, pool, nullptr);
}