#include "vkTextureCubemap.h"
#include "vkDeviceContext.h"
#include "vkCommon.h"
#include "vkMemory.h"
#include "vkImageViewCache.h"
#include "rhi/rhiTextureView.h"
#include "rhi/rhiCommandList.h"

vkTextureCubemap::vkTextureCubemap(vkDeviceContext* context, const rhiTextureDesc& desc)
    : rhiTextureCubeMap(desc),
    device(context->device),
    allocator(context->allocator),
    allocation(VK_NULL_HANDLE),
    image(VK_NULL_HANDLE),
    imgview_cache(context->get_imageview_cache())
{
    format = vk_format(desc.format);

    const VkImageCreateInfo image_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = { desc.width, desc.height, 1 },
        .mipLevels = desc.mips,
        .arrayLayers = desc.layers,
        .samples = vk_sample(desc.samples),
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | 
            VK_IMAGE_USAGE_SAMPLED_BIT | 
            VK_IMAGE_USAGE_STORAGE_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | 
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    const VmaAllocationCreateInfo vma_create_info{
        .usage = VMA_MEMORY_USAGE_AUTO
    };
    VK_CHECK_ERROR(vmaCreateImage(context->allocator, &image_create_info, &vma_create_info, &image, &allocation, nullptr));

    if (const auto ptr = imgview_cache.lock())
    {
        for (u32 i = 0; i < desc.mips; ++i)
        {
            const viewKey key{
                .image = image,
                .format = format,
                .aspect = vk_aspect_from_format(desc.format),
                .base_mip = i,
                .mip_count = 1,
                .base_layer = 0,
                .layer_count = desc.layers
            };
            ptr->get_or_create(key);
        }

        const viewKey key{
                .image = image,
                .format = format,
                .aspect = vk_aspect_from_format(desc.format),
                .base_mip = 0,
                .mip_count = desc.mips,
                .base_layer = 0,
                .layer_count = desc.layers,
                .is_cubemap = true
        };
        cube_view = ptr->get_or_create(key);
    }
}

vkTextureCubemap::~vkTextureCubemap()
{
    if (const auto ptr = imgview_cache.lock())
    {
        for (u32 i = 0; i < desc.layers; ++i)
        {
            ptr->delete_imageview(image);
        }
    }
    vmaDestroyImage(allocator, image, allocation);
}

VkImageView vkTextureCubemap::get_mip_view(const u32 base_mip) const
{
    const auto ptr = imgview_cache.lock();
    const viewKey key{
        .image = image,
        .format = format,
        .aspect = vk_aspect_from_format(desc.format),
        .base_mip = base_mip,
        .mip_count = 1,
        .base_layer = 0,
        .layer_count = desc.layers
    };
    return ptr->get_or_create(key);
}
