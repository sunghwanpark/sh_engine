#pragma once

#include "pch.h"
#include "rhi/rhiQueue.h"

struct rhiSubmitInfo;
class vkQueue : public rhiQueue
{
public:
	vkQueue(VkDevice device, rhiQueueType t, const u32 queue_family_index);
};