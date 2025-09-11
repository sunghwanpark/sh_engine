#pragma once

#include "pch.h"

struct vkDescriptorSetLayoutHolder 
{
public:
    vkDescriptorSetLayoutHolder(VkDevice device);
    ~vkDescriptorSetLayoutHolder();

    VkDevice device;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

struct vkDescriptorPoolHolder 
{
public:
    vkDescriptorPoolHolder(VkDevice device);
    ~vkDescriptorPoolHolder();

    VkDevice device;
    VkDescriptorPool pool = VK_NULL_HANDLE;
};