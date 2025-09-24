#pragma once

#include "pch.h"
#include "rhi/rhiGraphicsQueue.h"
#include "rhi/rhiSynchroize.h"
#include "rhi/rhiCommandList.h"

class rhiDeviceContext;
class rhiSwapChain;

struct rhiFrameSync
{
    std::unique_ptr<rhiSemaphore> image_available;
    std::unique_ptr<rhiSemaphore> present_ready;
    std::unique_ptr<rhiFence> in_flight;
};

class rhiFrameContext 
{
public:
    ~rhiFrameContext();

public:
    void submit(rhiDeviceContext* context, const u32 image_index);
    void update_inflight(rhiDeviceContext* context, const u32 frame_count);
    void acquire_next_image(u32* next_image);
    void wait(rhiDeviceContext* context);
    void reset(rhiDeviceContext* context);
    void present(const u32 image_index);

    void command_begin();
    void command_end();

    rhiCommandList* get_command_list(u32 q_family_idx);
    rhiCommandList* get_command_list(rhiQueueType type);
    const u32 get_frame_size();

public:
    rhiSwapChain* swapchain = nullptr;

protected:
    using frames = std::vector<std::unique_ptr<rhiCommandList>>;
    std::map<u32, std::unique_ptr<frames>, std::greater<>> frames_by_index;
    std::unordered_map<rhiQueueType, std::reference_wrapper<frames>> queue_frame;
    std::vector<rhiFrameSync> frame_sync;
    std::unique_ptr<rhiSemaphore> device_sync;

    u32 frame_index = 0;
    u32 cur_image_index = 0;
   
private:
    u64 device_timeline_value = 0;
}; 