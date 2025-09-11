#pragma once

#include "pch.h"
#include "rhi/rhiDefs.h"
#include "rhi/rhiShader.h"

class rhiSampler;
class rhiTexture;
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
    u32 mip = 0;
    u32 base_layer = 0;
    u32 layer_count = 1;
    bool is_separate_depth_view = false;
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