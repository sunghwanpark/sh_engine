#include "vkBuffer.h"
#include "vkCommon.h"
#include "vkDeviceContext.h"
#include "rhi/rhiDefs.h"

vkBuffer::vkBuffer(vkDeviceContext* context, const rhiBufferDesc& desc)
	: device(context->device), allocator(context->allocator), sz(desc.size)
{
    const VkBufferUsageFlags usage = get_vk_buffer_usage(desc.usage);

    VkBufferCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = desc.size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo vma_alloc_create_info{};
    if (desc.memory == rhiMem::auto_host) 
    {
        vma_alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        vma_alloc_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
    else 
    {
        vma_alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    }

    VmaAllocationInfo alloc_info;
    VK_CHECK_ERROR(vmaCreateBuffer(context->allocator, &create_info, &vma_alloc_create_info, &buffer, &alloc, &alloc_info));
    if (desc.memory == rhiMem::auto_host)
    {
        mapped = alloc_info.pMappedData;
    }
}

vkBuffer::~vkBuffer()
{
    vmaDestroyBuffer(allocator, buffer, alloc);
}

void* vkBuffer::map()
{
    if (!mapped) 
    {
        VK_CHECK_ERROR(vmaMapMemory(allocator, alloc, &mapped));
        explicitly_mapped = true;
    }
    return mapped;
}

void vkBuffer::flush(const u64 offset, const u64 bytes)
{
    VK_CHECK_ERROR(vmaFlushAllocation(allocator, alloc, offset, bytes));
}

void vkBuffer::unmap()
{
    if (!mapped)
        return;

    if(explicitly_mapped)
    {
        vmaUnmapMemory(allocator, alloc);
        explicitly_mapped = false;
        mapped = nullptr;
    }
}

u64 vkBuffer::size() const
{
    return sz;
}