#include "rhiFrameContext.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiSubmitInfo.h"
#include "rhi/rhiSwapChain.h"

rhiFrameContext::~rhiFrameContext()
{
    std::ranges::for_each(frames, [](rhiFrameInFlight& inflight)
        {
            inflight.cmd.reset();
            inflight.image_available.reset();
            inflight.render_finished.reset();
            inflight.in_flight.reset();
        });
    graphics_queue.reset();
    frames.clear();
}

rhiFrameInFlight& rhiFrameContext::get(const u32 image_index) 
{
    return frames[image_index]; 
}

void rhiFrameContext::acquire_next_image(u32* next_image)
{
    swapchain->acquire_next_image(next_image, frames[frame_index].image_available.get());
}

void rhiFrameContext::wait(rhiDeviceContext* context)
{
    context->wait(frames[frame_index].in_flight.get());
}

void rhiFrameContext::reset(rhiDeviceContext* context)
{
    context->reset(frames[frame_index].in_flight.get());
}

void rhiFrameContext::submit(const u32 image_index)
{
    const rhiSubmitInfo submit_info
    {
        .cmds = frames[image_index].cmd.get(),
        .wait_semaphore = frames[image_index].image_available.get(),
        .signal_semaphore = frames[image_index].render_finished.get(),
        .fence = frames[image_index].in_flight.get()
    };
    graphics_queue->submit(submit_info);
}

void rhiFrameContext::present(const u32 image_index)
{
    swapchain->present(image_index, frames[image_index].render_finished.get());
    frame_index = (frame_index + 1) % static_cast<u32>(frames.size());
}

const u32 rhiFrameContext::get_frame_size() const 
{ 
    return static_cast<u32>(frames.size()); 
}