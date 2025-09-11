#pragma once

#include "pch.h"
#include "rhi/rhiDefs.h"

constexpr int kMaxFramesInFlight = 3;

struct rhiRenderTargetView;
class rhiSwapChain
{
public:
    rhiSwapChain(u32 width, u32 height) : _width(width), _height(height) {}

public:
    virtual void acquire_next_image(u32* outIndex, class rhiSemaphore* signal) = 0;
    virtual void present(u32 imageIndex, class rhiSemaphore* wait) = 0;

    virtual u32 width() const { return _width; }
    virtual u32 height() const { return _height; }
    virtual rhiFormat format() const = 0;
    virtual const std::vector<rhiRenderTargetView>& views() const = 0;
    u32 get_swapchain_image_count() const { return image_count; }

protected:
    u32 _width, _height;
    u32 image_count = 0;
};