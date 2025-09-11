#pragma once

#include "pch.h"
#include "rhi/rhiBuffer.h"

class vkDeviceContext;
class vkBuffer final : public rhiBuffer
{
public:
	vkBuffer(vkDeviceContext* context, const rhiBufferDesc& desc);
	virtual ~vkBuffer();
	virtual void* map() final override;
	virtual void flush(const u64 offset, const u64 bytes) final override;
	virtual void  unmap() final override;
	virtual void* native() final override { return reinterpret_cast<void*>(buffer); }
	virtual u64 size() const final override;

	VkBuffer handle() { return buffer; }

private:
	VkDevice device;
	VmaAllocator allocator;
	VkBuffer buffer;
	VmaAllocation alloc;
	u64 sz;
	void* mapped = nullptr;
	bool explicitly_mapped = false;
};