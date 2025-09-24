#include "vkQueue.h"
#include "vkCommon.h"
#include "vkSynchronize.h"
#include "vkCommandList.h"
#include "rhi/rhiSubmitInfo.h"

vkQueue:: vkQueue(VkDevice device, rhiQueueType t, const u32 queue_family_index)
    : rhiQueue(t)
{
	VkQueue queue;
	vkGetDeviceQueue(device, queue_family_index, 0, &queue);
	native = queue;
	family_index = queue_family_index;
}
