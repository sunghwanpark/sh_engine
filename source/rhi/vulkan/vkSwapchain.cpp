#include "vkSwapchain.h"
#include "vkDeviceContext.h"
#include "vkCmdCenter.h"
#include "vkCommon.h"
#include "vkSynchronize.h"
#include "vkTexture.h"
#include "rhi/rhiDefs.h"

vkSwapchain::vkSwapchain(vkSwapChainCreateDesc&& desc)
	: rhiSwapChain(desc.width, desc.height), desc(desc)
{
	create_swapchain();
}

vkSwapchain::~vkSwapchain()
{
	textures.clear();
}

void vkSwapchain::acquire_next_image(u32* outIndex, class rhiSemaphore* signal)
{
	VkSemaphore semaphore = VK_NULL_HANDLE;
	if (signal)
		semaphore = static_cast<vkSemaphore*>(signal)->handle();

	VK_CHECK_ERROR(vkAcquireNextImageKHR(desc.context->device, swapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE, outIndex));
}

void vkSwapchain::present(u32 imageIndex, rhiSemaphore* wait)
{
	VkSemaphore semaphore = VK_NULL_HANDLE;
	if (wait)
		semaphore = static_cast<vkSemaphore*>(wait)->handle();

	const VkPresentInfoKHR present_info{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = semaphore ? 1u : 0u,
		.pWaitSemaphores = semaphore ? &semaphore : nullptr,
		.swapchainCount = 1,
		.pSwapchains = &swapchain,
		.pImageIndices = &imageIndex
	};

	VK_CHECK_ERROR(vkQueuePresentKHR(desc.present_queue, &present_info));
}

void vkSwapchain::recreate(u32 width, u32 height)
{
	vkDeviceWaitIdle(desc.context->device);

	textures.clear();
	rt_views.clear();
	vkDestroySwapchainKHR(desc.context->device, swapchain, nullptr);
	
	create_swapchain();
}

rhiFormat vkSwapchain::format() const
{
	return format(vk_format);
}

rhiFormat vkSwapchain::format(VkFormat format) const
{
	switch (format)
	{
	case VK_FORMAT_B8G8R8A8_UNORM: return rhiFormat::BGRA8_UNORM;
	case VK_FORMAT_B8G8R8A8_SRGB:  return rhiFormat::BGRA8_SRGB;
	case VK_FORMAT_R8G8B8A8_UNORM: return rhiFormat::RGBA8_UNORM;
	case VK_FORMAT_R8G8B8A8_SRGB:  return rhiFormat::RGBA8_SRGB;
	}
	return rhiFormat::BGRA8_UNORM;
}


void vkSwapchain::create_swapchain()
{
	VkSurfaceCapabilitiesKHR surface_capabilities;
	VK_CHECK_ERROR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(desc.context->phys_device, desc.surface, &surface_capabilities));

	VkSurfaceFormatKHR choose_format = get_surface_format();
	vk_format = choose_format.format;
	VkPresentModeKHR choose_present_mode = get_present_mode();
	VkExtent2D extent = get_extent(surface_capabilities, desc.width, desc.height);

	constexpr u32 desired_count = 3;
	image_count= std::max(desired_count, surface_capabilities.minImageCount);
	if (surface_capabilities.maxImageCount > 0 && image_count > surface_capabilities.maxImageCount)
		image_count = surface_capabilities.maxImageCount;

	std::array<u32, 2> queue_findices = { desc.gfx_family, desc.present_family };
	VkSwapchainCreateInfoKHR create_info
	{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = desc.surface,
		.minImageCount = image_count,
		.imageFormat = choose_format.format,
		.imageColorSpace = choose_format.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = queue_findices[0] == queue_findices[1] ? 0u : 2u,
		.pQueueFamilyIndices = queue_findices.data(),
		.preTransform = surface_capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = choose_present_mode,
		.clipped = VK_TRUE,
		.oldSwapchain = VK_NULL_HANDLE,
	};

	VK_CHECK_ERROR(vkCreateSwapchainKHR(desc.context->device, &create_info, nullptr, &swapchain));
	u32 swapchain_image_count = 0;
	VK_CHECK_ERROR(vkGetSwapchainImagesKHR(desc.context->device, swapchain, &swapchain_image_count, nullptr));
	images.resize(swapchain_image_count);
	VK_CHECK_ERROR(vkGetSwapchainImagesKHR(desc.context->device, swapchain, &swapchain_image_count, images.data()));

	textures.reserve(swapchain_image_count);
	rt_views.reserve(swapchain_image_count);
	for (u32 i = 0; i < swapchain_image_count; ++i)
	{
		std::unique_ptr<vkTexture> tex = std::make_unique<vkTexture>(desc.context, rhiTextureDesc{
			.width = extent.width,
			.height = extent.height,
			.layers = 1,
			.mips = 1,
			.format = format(choose_format.format)
			}, true);
		tex->bind_external_image(images[i]);
		textures.push_back(std::move(tex));

		const rhiRenderTargetView rt_view{
			.texture = textures[i].get(),
			.mip = 0,
			.base_layer = 0,
			.layer_count = 1,
			.layout = rhiImageLayout::color_attachment
		};
		rt_views.push_back(rt_view);
	}
}

VkSurfaceFormatKHR vkSwapchain::get_surface_format()
{
	VkSurfaceFormatKHR choose_format;
	u32 format_count = 0;
	VK_CHECK_ERROR(vkGetPhysicalDeviceSurfaceFormatsKHR(desc.context->phys_device, desc.surface, &format_count, nullptr));
	if (format_count > 0)
	{
		std::vector<VkSurfaceFormatKHR> surface_format(format_count);
		VK_CHECK_ERROR(vkGetPhysicalDeviceSurfaceFormatsKHR(desc.context->phys_device, desc.surface, &format_count, surface_format.data()));

		auto iter_surface_format = surface_format | std::views::filter([](const VkSurfaceFormatKHR& format)
			{
				return format.format == VK_FORMAT_R8G8B8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			});
		if (iter_surface_format.begin() != iter_surface_format.end())
		{
			choose_format = *iter_surface_format.begin();
		}
		else
		{
			assert(false);
		}
	}
	else
	{
		assert(false);
	}

	return choose_format;
}

VkPresentModeKHR vkSwapchain::get_present_mode()
{
	VkPresentModeKHR choose_present_mode;
	u32 present_mode_count = 0;
	VK_CHECK_ERROR(vkGetPhysicalDeviceSurfacePresentModesKHR(desc.context->phys_device, desc.surface, &present_mode_count, nullptr));
	if (present_mode_count > 0)
	{
		std::vector<VkPresentModeKHR> present_mode(present_mode_count);
		VK_CHECK_ERROR(vkGetPhysicalDeviceSurfacePresentModesKHR(desc.context->phys_device, desc.surface, &present_mode_count, present_mode.data()));
		auto iter_present_mode = present_mode | std::views::filter([](const VkPresentModeKHR& mode)
			{
				return mode == VK_PRESENT_MODE_MAILBOX_KHR;
			});
		if (iter_present_mode.begin() != iter_present_mode.end())
		{
			choose_present_mode = *iter_present_mode.begin();
		}
		else
		{
			assert(false);
		}
	}
	else
	{
		assert(false);
	}

	return choose_present_mode;
}

VkExtent2D vkSwapchain::get_extent(const VkSurfaceCapabilitiesKHR& capabilities, const u32 width, const u32 height)
{
	VkExtent2D extent = { width, height };
	extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	_width = extent.width;
	_height = extent.height;
	return extent;
}
