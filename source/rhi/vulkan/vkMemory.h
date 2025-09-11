#pragma once

#include "pch.h"

class vkMemory
{
public:
	static u32 get_memory_type(VkPhysicalDevice phys_device, u32 memory_type_bits, VkMemoryPropertyFlags flags);
};