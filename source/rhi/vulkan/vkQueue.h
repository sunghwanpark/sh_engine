#pragma once

#include "pch.h"

class vkQueue
{
public:
	void create_queue(VkDevice device, const u32 queue_family_index);
	VkQueue handle() { return queue; }

private:
	VkQueue queue;
};