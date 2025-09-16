#include "vkImageViewCache.h"
#include "vkCommon.h"
#include "vkTexture.h"
#include "rhi/rhiTextureView.h"
#include "rhi/rhiRenderpass.h"

bool viewKey::operator==(const viewKey& o) const noexcept
{
	return image == o.image && format == o.format && aspect == o.aspect &&
        base_mip == o.base_mip && mip_count == o.mip_count && base_layer == o.base_layer &&
        layer_count == o.layer_count && is_cubemap && o.is_cubemap;
}

size_t viewKeyHash::operator()(const viewKey& k) const noexcept
{
    size_t h = std::hash<u64>()((u64)k.image);
    h ^= std::hash<u64>()(((u64)k.format << 32) | k.aspect) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<u32>()(k.base_mip) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<u32>()(k.mip_count) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<u32>()(k.base_layer) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<u32>()(k.layer_count) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<bool>()(k.is_cubemap) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

vkImageViewCache::~vkImageViewCache()
{
    clear();
}

void vkImageViewCache::clear()
{
    cache.clear();
}

VkImageView vkImageViewCache::get_or_create(const rhiRenderTargetView* view)
{
    auto vk_tex = static_cast<vkTexture*>(view->texture);
    const VkImage image = vk_tex->get_image();
    const VkFormat vk_format = vk_tex->get_format();
    VkImageAspectFlags aspect = vk_aspect_from_format(view->texture->desc.format);

    const viewKey key{ 
        .image = image,
        .format = vk_format,
        .aspect = aspect,
        .base_mip = 0,
        .mip_count = 1,
        .base_layer = view->base_layer,
        .layer_count = view->layer_count
    };
    if (cache.contains(key))
        return cache[key];

    const VkImageViewType view_type = (view->layer_count > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    const VkImageSubresourceRange sub_res_range{
        .aspectMask = aspect,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = view->base_layer,
        .layerCount = view->layer_count,
    };
    const VkImageViewCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = view_type,
        .format = vk_format,
        .subresourceRange = sub_res_range
    };
    VkImageView vk_view = VK_NULL_HANDLE;
    VK_CHECK_ERROR(vkCreateImageView(device, &create_info, nullptr, &vk_view));
    cache.emplace(key, vk_view);
    reverse[key.image].push_back(vk_view);
    return vk_view;
}

VkImageView vkImageViewCache::get_or_create(const viewKey& key, const VkComponentMapping* swizzle) 
{
    if (cache.contains(key))
        return cache[key];

    const VkImageSubresourceRange sub_res_range{
       .aspectMask = key.aspect,
       .baseMipLevel = key.base_mip,
       .levelCount = key.mip_count,
       .baseArrayLayer = key.base_layer,
       .layerCount = key.layer_count,
    };
    
    const VkImageViewType view_type = key.is_cubemap ? VK_IMAGE_VIEW_TYPE_CUBE : key.layer_count > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    VkImageViewCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = key.image,
        .viewType = view_type,
        .format = key.format,
        .subresourceRange = sub_res_range
    };
    if (swizzle) 
        create_info.components = *swizzle;

    VkImageView view;
    VK_CHECK_ERROR(vkCreateImageView(device, &create_info, nullptr, &view));
    
    cache.emplace(key, view);
    reverse[key.image].push_back(view);
    return view;
}

void vkImageViewCache::delete_imageview(VkImage image)
{
    if (reverse.contains(image))
        return;

    if (auto node = reverse.extract(image); node) 
    {
        for (VkImageView v : node.mapped()) 
        {
            vkDestroyImageView(device, v, nullptr);
        }
        std::erase_if(cache, [&](const auto& kv) 
            {
            return kv.first.image == image;
            });
        return;
    }

    std::erase_if(cache, [&](auto& kv) 
        {
            if (kv.first.image == image) 
            {
                vkDestroyImageView(device, kv.second, nullptr);
                return true;
            }
            return false;
        });
}