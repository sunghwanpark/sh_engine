#include "rhiBindlessTable.h"

rhiBindlessHandle rhiBindlessTable::register_sampled_image(rhiTexture* tex, u32 base_mip)
{
    const bindlessTextureKey key{ .ptr = tex, .base_mip = base_mip };
    if (texture_index_cache.contains(key))
    {
        return { rhiBindlessClass::sampled_image, texture_index_cache[key] };
    }
    create_sampled_image(tex, base_mip);
    texture_index_cache.emplace(key, next_image);
    return { rhiBindlessClass::sampled_image, next_image++ };
}

rhiBindlessHandle rhiBindlessTable::register_sampler(rhiSampler* sampler)
{
    const rhiSamplerKey key = sampler->desc;
    if (sampler_index_cache.contains(key))
    {
        return { rhiBindlessClass::sampler, sampler_index_cache[key] };
    }
    create_sampler(sampler);
    sampler_index_cache.emplace(key, next_sampler);
    return { rhiBindlessClass::sampler, next_sampler++ };
}