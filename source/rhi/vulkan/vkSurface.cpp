#include "vkSurface.h"
#include "vkCommon.h"
#include "glfw3.h"

void vkSurface::create_surface(VkInstance instance, GLFWwindow* window)
{
	VK_CHECK_ERROR(glfwCreateWindowSurface(instance, window, nullptr, &surface));
}