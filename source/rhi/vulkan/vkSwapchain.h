#pragma once

#include "pch.h"
#include "rhi/rhiSwapChain.h"

class vkDeviceContext;
class vkFrameBuffer;
class vkTexture;
struct vkSwapChainCreateDesc 
{
	vkDeviceContext* context;
	VkSurfaceKHR surface;
	u32 gfx_family = 0;
	u32 present_family = 0;
	VkQueue present_queue = VK_NULL_HANDLE;

	u32 width = 0;
	u32 height = 0;
};

class vkSwapchain : public rhiSwapChain
{
public:
	explicit vkSwapchain(vkSwapChainCreateDesc&& desc);
	~vkSwapchain();

public:
	void acquire_next_image(u32* outIndex, class rhiSemaphore* signal) override;
	void present(u32 imageIndex, class rhiSemaphore* wait) override;
	void recreate(u32 width, u32 height);

	rhiFormat format() const override;
	const std::vector<rhiRenderTargetView>& views() const override { return rt_views; }

private:
	void create_swapchain();
	VkSurfaceFormatKHR get_surface_format();
	VkPresentModeKHR get_present_mode();
	VkExtent2D get_extent(const VkSurfaceCapabilitiesKHR& capabilities, const u32 width, const u32 height);
	rhiFormat format(VkFormat format) const;

private:
	vkSwapChainCreateDesc desc;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat vk_format;

	std::vector<VkImage> images;
	std::vector<std::unique_ptr<vkTexture>> textures;
	std::vector<rhiRenderTargetView> rt_views;
};