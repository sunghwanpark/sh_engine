#pragma once

#include "pch.h"
#include "rhi/rhiCmdCenter.h"

struct GLFWwindow;
class vkSurface;
class vkSwapchain;
class vkQueue;

struct vkQueueFamilyIndices
{
	enum class queue_family_type : uint8
	{
		graphics,
		compute,
		transfer,
		present,
		count
	};

	std::array<std::optional<u32>, 4> families;
	std::array<std::vector<float>, 4> priorities;

	const bool is_complete() const
	{
		bool all_has_value = std::ranges::all_of(families, [](const auto& q)
			{
				return q.has_value();
			});
		return all_has_value;
	}
};

class vkCmdCenter final : public rhiCmdCenter
{
public:
	vkCmdCenter();
	virtual ~vkCmdCenter();

public:
	bool initialize(std::string_view application_name, GLFWwindow* window, const u32 width, const u32 height) final override;

private:
	void volk_init();
	void create_instance(std::string_view application_name);
	std::vector<const char*> get_required_extensions();
	bool has_layer(const char** layers, const u32 size);

	void create_surface(GLFWwindow* window);

	bool create_physical_device();
	bool is_device_suitable(VkPhysicalDevice device);
	vkQueueFamilyIndices find_queue_families(VkPhysicalDevice device);

	bool create_logical_device();
	bool create_swapchain(const u32 width, const u32 height);

	void init_frame_context();

private:
	VkInstance instance = VK_NULL_HANDLE;

	vkQueueFamilyIndices queue_family_indices;
	std::unique_ptr<vkSurface> surface;
	std::unique_ptr<vkSwapchain> swapchain;
};
