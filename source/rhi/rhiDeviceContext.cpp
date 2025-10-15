#include "rhiDeviceContext.h"
#include "rhiQueue.h"
#include "rhiSubmitInfo.h"

const u32 rhiDeviceContext::get_queue_family_index(rhiQueueType type) const
{
	ASSERT(queue.contains(type));
	auto q = queue.at(type).get();
	return q->q_index();
}

rhiQueue* rhiDeviceContext::get_queue(rhiQueueType type) const
{
	ASSERT(queue.contains(type));
	return queue.at(type).get();
}

rhiQueueType rhiDeviceContext::get_queue_type(const u32 q_family_idx)
{
	rhiQueueType type = rhiQueueType::none;
	for (auto& [t, q] : queue)
	{
		if (q_family_idx == q->q_index())
			type = type | t;
	}
	return type;
}