#include "rhiFrameContext.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiSubmitInfo.h"
#include "rhi/rhiSwapChain.h"
#include "rhi/rhiQueue.h"

rhiFrameContext::~rhiFrameContext()
{
    queue_frame.clear();
}

void rhiFrameContext::update_inflight(rhiDeviceContext* context, const u32 frame_count)
{
    const auto& queue_families = context->get_queues();
    auto values = queue_families | std::views::values;
    for (auto& qptr : values)
    {
        const auto q_type = qptr->get_type();
        const u32 idx = qptr->q_index();

        auto [it, inserted] = frames_by_index.try_emplace(idx, nullptr);
        if (inserted)
        {
            auto vec = std::make_unique<frames>(frame_count);
            for (u32 i = 0; i < frame_count; ++i)
            {
                (*vec)[i] = context->create_commandlist(idx);
            }
            it->second = std::move(vec);
        }
        queue_frame.emplace(q_type, std::ref(*it->second));
    }

    frame_sync.resize(frame_count);
    for (u32 i = 0; i < frame_count; ++i)
    {
        frame_sync[i].image_available = context->create_semaphore();
        frame_sync[i].present_ready = context->create_semaphore();
        frame_sync[i].in_flight = context->create_fence(true);
    }

    device_sync = context->create_semaphore();
}

void rhiFrameContext::acquire_next_image(u32* next_image)
{
    // because last submit is graphics queue
    swapchain->acquire_next_image(next_image, frame_sync[frame_index].image_available.get());
}

void rhiFrameContext::wait(rhiDeviceContext* context)
{
    context->wait(frame_sync[frame_index].in_flight.get());
}

void rhiFrameContext::reset(rhiDeviceContext* context)
{
    context->reset(frame_sync[frame_index].in_flight.get());
}

void rhiFrameContext::command_begin()
{
    for (auto& [idx, frames_ptr] : frames_by_index) 
    {
        auto& frames = *frames_ptr;
        auto& cmd = frames[frame_index];
        cmd->reset();
        cmd->begin();
    }
}

void rhiFrameContext::command_end()
{
    for (auto& [idx, frames_ptr] : frames_by_index)
    {
        auto& frames = *frames_ptr;
        auto& cmd = frames[frame_index];
        cmd->end();
    }
}

void rhiFrameContext::submit(rhiDeviceContext* context, const u32 image_index)
{
    u64& v = device_timeline_value;

    auto common_wait_submitinfo = rhiSemaphoreSubmitInfo{
        .semaphore = frame_sync[frame_index].image_available.get(),
        .value = 0,
        .stage = rhiPipelineStage::color_attachment_output
    };
    auto common_signal_submitinfo = rhiSemaphoreSubmitInfo{
        .semaphore = frame_sync[frame_index].present_ready.get(),
        .value = 0,
        .stage = rhiPipelineStage::all_commands
    };

    for (auto& [idx, frames_ptr] : frames_by_index)
    {
        rhiQueueType type = context->get_queue_type(idx);
        // G C T
        if ((type & (rhiQueueType::graphics | rhiQueueType::compute | rhiQueueType::transfer)) == (rhiQueueType::graphics | rhiQueueType::compute | rhiQueueType::transfer))
        {
            const auto& frame = queue_frame.at(rhiQueueType::graphics).get();
            rhiSubmitInfo submit_info{
                .cmd_lists = { frame[frame_index].get()},
                .waits = { common_wait_submitinfo },
                .signals = { common_signal_submitinfo },
                .fence = frame_sync[frame_index].in_flight.get()
            };
            context->submit(rhiQueueType::graphics, submit_info);
        }
        // G C,
        else if ((type & (rhiQueueType::graphics | rhiQueueType::compute)) == (rhiQueueType::graphics | rhiQueueType::compute))
        {
            rhiSemaphoreSubmitInfo timeline_wait{
                .semaphore = device_sync.get(),
                .value = v,
                .stage = rhiPipelineStage::all_commands
            };
            const auto& frame = queue_frame.at(rhiQueueType::graphics).get();
            rhiSubmitInfo submit_info{
                .cmd_lists = { frame[frame_index].get()},
                .waits = { timeline_wait, common_wait_submitinfo },
                .signals = { common_signal_submitinfo },
                .fence = frame_sync[frame_index].in_flight.get()
            };
            context->submit(rhiQueueType::graphics, submit_info);
        }
        // T
        else if ((type & rhiQueueType::transfer) == rhiQueueType::transfer)
        {
            const auto& cur_frame = queue_frame.at(rhiQueueType::transfer).get();
            rhiSubmitInfo submit_info{
                .cmd_lists = { cur_frame[frame_index].get() },
                .signals = { rhiSemaphoreSubmitInfo{
                    .semaphore = device_sync.get(),
                    .value = ++v,
                    .stage = rhiPipelineStage::copy
                }}
            };
            context->submit(rhiQueueType::transfer, submit_info);
        }
        // C
        else if ((type & rhiQueueType::compute) == rhiQueueType::compute)
        {
            const auto& cur_frame = queue_frame.at(rhiQueueType::compute).get();
            rhiSubmitInfo submit_info{
                .cmd_lists = { cur_frame[frame_index].get() },
                .waits = { rhiSemaphoreSubmitInfo{
                    .semaphore = device_sync.get(),
                    .value = v,
                    .stage = rhiPipelineStage::compute_shader
                }},
                .signals = { rhiSemaphoreSubmitInfo{
                    .semaphore = device_sync.get(),
                    .value = ++v,
                    .stage = rhiPipelineStage::compute_shader
                }}
            };
            context->submit(rhiQueueType::compute, submit_info);
        }
        // G
        else if ((type & rhiQueueType::graphics) == rhiQueueType::graphics)
        {
            const auto& cur_frame = queue_frame.at(rhiQueueType::graphics).get();
            rhiSubmitInfo submit_info{
                .cmd_lists = { cur_frame[frame_index].get() },
                .waits = { rhiSemaphoreSubmitInfo{
                    .semaphore = device_sync.get(),
                    .value = v,
                    .stage = rhiPipelineStage::all_graphics
                }, common_wait_submitinfo },
                .signals = { common_signal_submitinfo },
                .fence = frame_sync[frame_index].in_flight.get()
            };
            context->submit(rhiQueueType::graphics, submit_info);
        }
    }
}

void rhiFrameContext::present(const u32 image_index)
{
    auto& frame = queue_frame.at(rhiQueueType::graphics).get();
    swapchain->present(image_index, frame_sync[image_index].present_ready.get());
    frame_index = (frame_index + 1) % static_cast<u32>(frame.size());
}

rhiCommandList* rhiFrameContext::get_command_list(u32 q_family_idx)
{
    assert(frames_by_index.contains(q_family_idx));
    auto frames = frames_by_index[q_family_idx].get();
    return (*frames)[frame_index].get();
}

rhiCommandList* rhiFrameContext::get_command_list(rhiQueueType type)
{
    assert(queue_frame.contains(type));

    auto& frames = queue_frame.at(type).get();
    return frames[frame_index].get();
}

const u32 rhiFrameContext::get_frame_size() 
{ 
    auto& frame = queue_frame.at(rhiQueueType::graphics).get();
    return static_cast<u32>(frame.size());
}