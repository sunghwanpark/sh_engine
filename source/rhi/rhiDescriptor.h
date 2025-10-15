#pragma once

#include "pch.h"
#include "rhi/rhiDefs.h"
#include "rhi/rhiShader.h"

class rhiSampler;
class rhiTexture;
class rhiTextureCubeMap;
class rhiBuffer;

struct rhiDescriptorSetLayoutBinding 
{
    u32 binding;
    rhiDescriptorType type;
    u32 count = 1;
    rhiShaderStage stage = rhiShaderStage::fragment;
};

struct rhiDescriptorPoolSize 
{
    rhiDescriptorType type;
    u32 count;
};

struct rhiDescriptorPoolCreateInfo
{
    std::vector<rhiDescriptorPoolSize> pool_sizes;
    rhiDescriptorPoolCreateFlags create_flags = rhiDescriptorPoolCreateFlags::free_descriptor_set;
};

struct rhiDescriptorSet
{
    void* native = nullptr;
    u32 set_index = 0;
};

struct rhiDescriptorPool
{
    void* native = nullptr;
};

struct rhiDescriptorSetLayout
{
    void* native = nullptr;
    u32 set_index = 0;
};

struct rhiDescriptorImageInfo 
{
    rhiSampler* sampler = nullptr;
    rhiTexture* texture = nullptr;
    rhiTextureCubeMap* texture_cubemap = nullptr;
    u32 mip = 0;
    u32 base_layer = 0;
    u32 layer_count = 1;
    bool is_separate_depth_view = false;
    rhiCubemapViewType cubemap_viewtype = rhiCubemapViewType::mip;
    rhiImageLayout layout = rhiImageLayout::shader_readonly;
};

struct rhiDescriptorBufferInfo 
{
    rhiBuffer* buffer = nullptr;
    u64 offset = 0;
    u64 range = 0;
};

struct rhiWriteDescriptor 
{
    rhiDescriptorSet set;
    u32 binding = 0;
    u32 array_index = 0;
    u32 count = 0;
    rhiDescriptorType type;
    
    std::vector<rhiDescriptorImageInfo> image;
    std::vector<rhiDescriptorBufferInfo> buffer;
};

struct rhiDescriptorIndexingElement
{
    u32 binding;
    rhiDescriptorType type;
    u32 descriptor_count;
    rhiShaderStage stage;
    rhiDescriptorBindingFlags binding_flags;
};

struct rhiDescriptorIndexing
{
    std::vector<rhiDescriptorIndexingElement> elements;
};

struct rhiPushConstant
{
    rhiShaderStage stage;
    u32 bytes;
};