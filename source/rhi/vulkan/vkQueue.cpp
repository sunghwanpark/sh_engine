#include "vkQueue.h"
#include "vkCommon.h"

void vkQueue::create_queue(VkDevice device, const u32 queue_family_index)
{
	vkGetDeviceQueue(device, queue_family_index, 0, &queue);
}