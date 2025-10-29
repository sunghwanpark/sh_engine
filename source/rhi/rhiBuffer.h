#pragma once
#include "rhi/rhiDefs.h"

struct rhiBufferDesc
{
    u64 size = 0;
    rhiBufferUsage usage;
    rhiMem memory = rhiMem::auto_device;
};

class rhiBuffer
{
public:
    virtual ~rhiBuffer() = default;
    virtual void* map() = 0;
    virtual void flush(const u64 offset, const u64 bytes) = 0;
    virtual void  unmap() = 0;
    virtual u64 size() const = 0;
    virtual void* native() = 0;
};
