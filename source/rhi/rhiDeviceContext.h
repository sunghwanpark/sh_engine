#pragma once

#include "pch.h"
#include "rhi/rhiBuffer.h"
#include "rhi/rhiTextureView.h"
#include "rhi/rhiSampler.h"
#include "rhi/rhiDescriptor.h"
#include "rhi/rhiPipeline.h"
#include "rhi/rhiBindlessTable.h"

class rhiCommandList;
class rhiFence;
class rhiSemaphore;
class rhiQueue;
struct rhiSubmitInfo;

class rhiDeviceContext 
{
public:
    virtual ~rhiDeviceContext() = default;

    virtual std::unique_ptr<rhiCommandList> create_commandlist(u32 queue_family) = 0;
    virtual std::unique_ptr<rhiBindlessTable> create_bindless_table(const rhiBindlessDesc& desc, const u32 set_index = 0) = 0;
    virtual std::unique_ptr<rhiBuffer> create_buffer(const rhiBufferDesc& desc) = 0;
    virtual std::unique_ptr<rhiTexture> create_texture(const rhiTextureDesc& desc) = 0;
    virtual std::unique_ptr<rhiTexture> create_texture_from_path(std::string_view path, bool is_hdr = false, bool srgb = true) = 0;
    virtual std::shared_ptr<rhiTextureCubeMap> create_texture_cubemap(const rhiTextureDesc& desc) = 0;
    virtual std::unique_ptr<rhiSampler> create_sampler(const rhiSamplerDesc& desc) = 0;
    virtual std::unique_ptr<rhiSemaphore> create_semaphore() = 0;
    virtual std::unique_ptr<rhiFence> create_fence(bool signaled) = 0;

    virtual rhiDescriptorSetLayout create_descriptor_set_layout(const std::vector<rhiDescriptorSetLayoutBinding>& bindings, u32 set_index = 0) = 0;
    virtual rhiDescriptorPool create_descriptor_pool(const rhiDescriptorPoolCreateInfo& create_info, u32 max_sets) = 0;
    virtual std::vector<rhiDescriptorSet> allocate_descriptor_sets(rhiDescriptorPool pool, const std::vector<rhiDescriptorSetLayout>& layouts) = 0;
    virtual std::vector<rhiDescriptorSet> allocate_descriptor_indexing_sets(rhiDescriptorPool pool, const std::vector<rhiDescriptorSetLayout>& layouts, const std::vector<u32>& counts) = 0;
    virtual void update_descriptors(const std::vector<rhiWriteDescriptor>& writes) = 0;
    virtual rhiDescriptorSetLayout create_descriptor_indexing_set_layout(const rhiDescriptorIndexing& desc, const u32 set_index) = 0;

    virtual rhiPipelineLayout create_pipeline_layout(std::vector<rhiDescriptorSetLayout> set_layouts, std::vector<rhiPushConstant> push_constant_bytes = {}, void** keep_alive_out = nullptr) = 0;
    virtual std::unique_ptr<rhiPipeline> create_graphics_pipeline(const rhiGraphicsPipelineDesc& desc, const rhiPipelineLayout& layout) = 0;
    virtual std::unique_ptr<rhiPipeline> create_compute_pipeline(const rhiComputePipelineDesc& desc, const rhiPipelineLayout& layout) = 0;
    virtual std::shared_ptr<rhiCommandList> begin_onetime_commands(rhiQueueType t = rhiQueueType::graphics) = 0;
    virtual void submit_and_wait(std::shared_ptr<rhiCommandList> cmd, rhiQueueType t = rhiQueueType::graphics) = 0;
    virtual void submit(rhiQueueType type, const rhiSubmitInfo& info) = 0;
    virtual void wait(class rhiFence* f) = 0;
    virtual void reset(class rhiFence* f) = 0;

    const u32 get_queue_family_index(rhiQueueType type) const;
    rhiQueue* get_queue(rhiQueueType type) const;
    rhiQueueType get_queue_type(const u32 q_family_idx);
    const std::unordered_map<rhiQueueType, std::shared_ptr<rhiQueue>>& get_queues() const { return queue; }

public:
    glm::vec2 viewport_size;

protected:
    std::unordered_map<rhiQueueType, std::shared_ptr<rhiQueue>> queue;
};
