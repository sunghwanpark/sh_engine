#include "vkTexture.h"
#include "vkDeviceContext.h"
#include "vkCommon.h"
#include "vkMemory.h"
#include "vkImageViewCache.h"
#include "rhi/rhiTextureView.h"
#include "rhi/rhiCommandList.h"

vkTexture::vkTexture(vkDeviceContext* context, const rhiTextureDesc& desc, bool external_image)
    : rhiTexture(desc), 
    device(context->device), 
    allocator(context->allocator), 
    allocation(VK_NULL_HANDLE), 
    image(VK_NULL_HANDLE), 
    imgview_cache(context->get_imageview_cache()), 
    external(external_image)
{
    format = vk_format(desc.format);

    if (!external)
    {
        const VkImageCreateInfo image_create_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = vk_format(desc.format),
            .extent = { desc.width, desc.height, 1 },
            .mipLevels = desc.mips,
            .arrayLayers = desc.layers,
            .samples = vk_sample(desc.samples),
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = desc.is_depth ? static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT) :
                                                static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        const VmaAllocationCreateInfo vma_create_info{
            .usage = VMA_MEMORY_USAGE_AUTO
        };
        VK_CHECK_ERROR(vmaCreateImage(context->allocator, &image_create_info, &vma_create_info, &image, &allocation, nullptr));

        if (const auto ptr = imgview_cache.lock())
        {
            const viewKey key{
                .image = image,
                .format = format,
                .aspect = vk_aspect_from_format(desc.format),
                .base_mip = 0,
                .mip_count = desc.mips,
                .base_layer = 0,
                .layer_count = desc.layers
            };
            view = ptr->get_or_create(key);
            if (desc.is_depth && desc.is_separate_depth_stencil)
            {
                const viewKey depth_view_key{
                    .image = image,
                    .format = format,
                    .aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .base_mip = 0,
                    .mip_count = desc.mips,
                    .base_layer = 0,
                    .layer_count = desc.layers
                };
                depth_view = ptr->get_or_create(depth_view_key);
            }
            if (desc.layers > 1)
            {
                layer_views.resize(desc.layers);
                for (u32 index = 0; index < desc.layers; ++index)
                {
                    const viewKey layer_key{
                        .image = image,
                        .format = format,
                        .aspect = vk_aspect_from_format(desc.format),
                        .base_mip = 0,
                        .mip_count = desc.mips,
                        .base_layer = index,
                        .layer_count = 1
                    };
                    layer_views[index] = ptr->get_or_create(layer_key);
                }
            }
        }
    }
}

vkTexture::vkTexture(vkDeviceContext* context, std::string_view path, bool is_hdr)
    : rhiTexture(context, path, is_hdr),
    device(context->device),
    allocator(context->allocator),
    allocation(VK_NULL_HANDLE),
    image(VK_NULL_HANDLE),
    imgview_cache(context->get_imageview_cache())
{
    format = vk_format(desc.format);

    const VkImageCreateInfo image_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = vk_format(desc.format),
        .extent = { desc.width, desc.height, 1 },
        .mipLevels = desc.mips,
        .arrayLayers = desc.layers,
        .samples = vk_sample(desc.samples),
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    const VmaAllocationCreateInfo vma_create_info{
        .usage = VMA_MEMORY_USAGE_AUTO
    };
    VK_CHECK_ERROR(vmaCreateImage(context->allocator, &image_create_info, &vma_create_info, &image, &allocation, nullptr));

    if(is_hdr)
        upload(context);

    if (const auto ptr = imgview_cache.lock())
    {
        const viewKey key{
            .image = image,
            .format = format,
            .aspect = vk_aspect_from_format(desc.format),
            .base_mip = 0,
            .mip_count = desc.mips,
            .base_layer = 0,
            .layer_count = desc.layers
        };
        view = ptr->get_or_create(key);
    }
    if(!is_hdr)
        generate_mips(context);
}

vkTexture::~vkTexture()
{
    if (const auto ptr = imgview_cache.lock())
    {
        ptr->delete_imageview(image);
    }
    if(!external)
        vmaDestroyImage(allocator, image, allocation);
}

bool vkTexture::is_depth() const
{
    switch (format)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;
    }
    return false;
}

void vkTexture::bind_external_image(VkImage image)
{
    this->image = image;
    if (const auto ptr = imgview_cache.lock())
    {
        const viewKey key{
            .image = image,
            .format = format,
            .aspect = vk_aspect_from_format(desc.format),
            .base_mip = 0,
            .mip_count = desc.mips,
            .base_layer = 0,
            .layer_count = desc.layers
        };
        view = ptr->get_or_create(key);
    }
}

VkImageView vkTexture::get_layer_view(const u32 idx) const
{ 
    assert(idx < layer_views.size());
    return layer_views[idx]; 
}

void vkTexture::upload(vkDeviceContext* context)
{
    auto cmd = context->begin_onetime_commands();
    assert(cmd);

    cmd->image_barrier(this, rhiImageLayout::undefined, rhiImageLayout::transfer_dst);
    const u32 bytes = desc.width * desc.height * 4 * sizeof(f32);
    auto staging = context->create_buffer(rhiBufferDesc{
        .size = bytes,
        .usage = rhiBufferUsage::transfer_src,
        .memory = rhiMem::auto_host
        });
    auto mapped = staging->map();
    std::memcpy(mapped, rgba.data(), bytes);
    staging->flush(0, bytes);
    staging->unmap();
    std::vector<rhiBufferImageCopy> regions = {
        rhiBufferImageCopy{
            .buffer_offset = 0,
            .buffer_rowlength = 0,
            .buffer_imageheight = 0,
            .imageSubresource = rhiImageSubresourceLayers{
                .aspect = rhiImageAspect::color
                },
            .image_offset = {0,0,0},
            .image_extent = { desc.width, desc.height, 1.f },
        } };
    cmd->copy_buffer_to_image(staging.get(), this, rhiImageLayout::transfer_dst, regions);
    cmd->image_barrier(this, rhiImageLayout::transfer_dst, rhiImageLayout::shader_readonly);
    context->submit_and_wait(cmd);
}