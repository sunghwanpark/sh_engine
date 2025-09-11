#pragma once

#include "pch.h"

class vkSurface
{
public:
	void create_surface(VkInstance instance, GLFWwindow* window);
	const VkSurfaceKHR& get_surface() const { return surface; }
private:
	VkSurfaceKHR surface;
};