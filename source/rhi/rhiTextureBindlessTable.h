#pragma once

#include "pch.h"
#include "rhi/rhiBindlessTable.h"
#include "rhi/rhiSampler.h"

struct rhiTextureBindlessDesc 
{
    u32 max_sampled_images = 65536;
    u32 max_samplers = 2048;
    bool update_afterbind = true; // Vulkan: UPDATE_AFTER_BIND, DX12: always ok
    bool partially_bound = true; // Vulkan: PARTIALLY_BOUND
    bool variable_count = true; // Vulkan: VARIABLE_DESCRIPTOR_COUNT
};

class rhiTexture;
class rhiCommandList;
struct rhiPipelineLayout;
struct rhiDescriptorSetLayout;

struct bindlessTextureKey
{
    const rhiTexture* ptr;
    u32 base_mip;
    bool operator==(const bindlessTextureKey& o) const noexcept
    {
        return ptr == o.ptr && base_mip == o.base_mip;
    }
};

struct bindlessTextureKeyHash
{
    size_t operator()(const bindlessTextureKey& k) const noexcept
    {
        return std::hash<const void*>()(k.ptr) ^ (size_t(k.base_mip) * 0x85eb);
    }
};

class rhiTextureBindlessTable : public rhiBindlessTable
{
public:
    virtual ~rhiTextureBindlessTable() = default;

    rhiBindlessHandle register_sampled_image(rhiTexture* tex, u32 base_mip = 0);
    rhiBindlessHandle register_sampler(rhiSampler* sampler);

    virtual void create_sampled_image(rhiTexture* tex, u32 base_mip = 0) = 0;
    virtual void create_sampler(rhiSampler* sampler) = 0;

    virtual void update_sampled_image(rhiBindlessHandle h, rhiTexture* tex, u32 base_mip = 0) = 0;

protected:
    std::unordered_map<bindlessTextureKey, u32, bindlessTextureKeyHash> texture_index_cache;
    std::unordered_map<rhiSamplerKey, u32, rhiSamplerKeyHash> sampler_index_cache;

    u32 next_image = 1;
    u32 next_sampler = 1;
};
