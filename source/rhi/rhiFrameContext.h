#pragma once

#include "pch.h"
#include "rhi/rhiGraphicsQueue.h"
#include "rhi/rhiSynchroize.h"
#include "rhi/rhiCommandList.h"

class rhiDeviceContext;
class rhiSwapChain;

struct rhiFrameInFlight 
{
    std::unique_ptr<rhiCommandList> cmd;
    std::unique_ptr<rhiSemaphore> image_available; // acquire -> graphics
    std::unique_ptr<rhiSemaphore> render_finished; // graphics -> present
    std::unique_ptr<rhiFence> in_flight;       // CPU <-> GPU
};

class rhiFrameContext 
{
public:
    ~rhiFrameContext();

    rhiFrameInFlight& get(const u32 image_index);
    void acquire_next_image(u32* next_image);
    void wait(rhiDeviceContext* context);
    void reset(rhiDeviceContext* context);
    void submit(const u32 image_index);
    void present(const u32 image_index);
    const u32 get_frame_size() const;

public:
    rhiSwapChain* swapchain = nullptr;

protected:
    std::unique_ptr<class rhiGraphicsQueue> graphics_queue;
    std::vector<rhiFrameInFlight> frames;
    u32 frame_index = 0;
    u32 cur_image_index = 0;
}; 