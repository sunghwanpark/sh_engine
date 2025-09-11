#include "vkMemory.h"

u32 vkMemory::get_memory_type(VkPhysicalDevice phys_device, u32 memory_type_bits, VkMemoryPropertyFlags flags)
{
	VkPhysicalDeviceMemoryProperties memory_prop;
	vkGetPhysicalDeviceMemoryProperties(phys_device, &memory_prop);
	for (u32 index = 0; index < memory_prop.memoryTypeCount; ++index)
	{
		const bool supported = (memory_type_bits & (1u << index)) != 0;
		const bool matched = (memory_prop.memoryTypes[index].propertyFlags & flags) == flags;
		if (supported && matched)
			return index;
	}
	assert(false);
	return -1;
}