#pragma once

#include "pch.h"
#include "rhi/rhiDefs.h"

class rhiQueue
{
public:
	rhiQueue(rhiQueueType t) : type(t) {}

public:
	void* handle() { return native; }
	const rhiQueueType get_type() const { return type; }
	const u32 q_index() const { return family_index; }

protected:
	rhiQueueType type;
	void* native;
	u32 family_index;
};