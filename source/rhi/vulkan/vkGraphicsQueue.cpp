#include "vkGraphicsQueue.h"
#include "vkCommon.h"
#include "vkSynchronize.h"
#include "vkCommandList.h"
#include "rhi/rhiSubmitInfo.h"

void vkGraphicsQueue::submit(const rhiSubmitInfo& info)
{
    auto vk_wait_semaphore = static_cast<vkSemaphore*>(info.wait_semaphore);
    std::array<VkSemaphoreSubmitInfo, 1> wait_semaphores = {
        VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = vk_wait_semaphore->handle(),
            .value = 0,
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0
        }
    };
    auto vk_signal_semaphore = static_cast<vkSemaphore*>(info.signal_semaphore);
    std::array<VkSemaphoreSubmitInfo, 1> signal_semaphores = {
        VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = vk_signal_semaphore->handle(),
            .value = 0,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .deviceIndex = 0
        }
    };
    auto vk_cmd = static_cast<vkCommandList*>(info.cmds);
    std::array<VkCommandBufferSubmitInfo, 1> cmds = {
        VkCommandBufferSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = vk_cmd->get_cmd_buffer(),
            .deviceMask = 0
        }
    };

    const VkSubmitInfo2 submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = wait_semaphores.data(),
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = cmds.data(),
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = signal_semaphores.data()
    };

    VkFence vk_fence = VK_NULL_HANDLE;
    if (info.fence) 
    {
        vk_fence = static_cast<vkFence*>(info.fence)->handle();
    }

    VK_CHECK_ERROR(vkQueueSubmit2(queue, 1, &submit_info, vk_fence));
}
