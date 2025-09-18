#pragma once

#include "pch.h"
#include "rhi/rhiTextureView.h"

class vkDeviceContext;
class vkImageViewCache;
class vkTexture final : public rhiTexture
{
public:
	vkTexture(vkDeviceContext* context, const rhiTextureDesc& desc, bool external_image = false);
	vkTexture(vkDeviceContext* context, std::string_view path, bool is_hdr = false, bool srgb = true);
	virtual ~vkTexture();

public:
	void bind_external_image(VkImage image);
	VkImage get_image() const { return image; }
	VkFormat get_format() const { return format; }
	VkImageView get_view() const { assert(view != VK_NULL_HANDLE); return view; }
	VkImageView get_depth_view() const { assert(depth_view != VK_NULL_HANDLE); return depth_view; }
	VkImageView get_layer_view(const u32 idx) const;
	bool is_depth() const;

private:
	void upload(vkDeviceContext* context);

private:
	VkDevice device;
	VmaAllocator allocator;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VkImage image = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkImageView depth_view = VK_NULL_HANDLE;
	std::vector<VkImageView> layer_views;
	VkFormat format;
	bool external = false;

	std::weak_ptr<vkImageViewCache> imgview_cache;
};
