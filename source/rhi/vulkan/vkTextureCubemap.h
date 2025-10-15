#pragma once

#include "pch.h"
#include "rhi/rhiTextureView.h"

class vkDeviceContext;
class vkImageViewCache;
class vkTextureCubemap final : public rhiTextureCubeMap
{
public:
	vkTextureCubemap(vkDeviceContext* context, const rhiTextureDesc& desc);
	virtual ~vkTextureCubemap();

public:
	VkImage get_image() const { return image; }
	VkFormat get_format() const { return format; }
	VkImageView get_cube_view() const { ASSERT(cube_view != VK_NULL_HANDLE); return cube_view; }
	VkImageView get_mip_view(const u32 base_mip) const;

private:
	VkDevice device;
	VmaAllocator allocator;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VkImage image = VK_NULL_HANDLE;
	VkImageView cube_view = VK_NULL_HANDLE;
	std::vector<VkImageView> mip_views;
	VkFormat format;

	std::weak_ptr<vkImageViewCache> imgview_cache;
};
