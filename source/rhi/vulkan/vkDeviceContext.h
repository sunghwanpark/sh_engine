#pragma once

#include "pch.h"
#include "rhi/rhiDeviceContext.h"
#include "vkCmdCenter.h"
#include "vk_mem_alloc.h"

class rhiBuffer;
class rhiCommandList;
struct rhiBufferDesc;
struct rhiTextureDesc;
struct rhiSamplerDesc;
struct vkDescriptorSetLayoutHolder;
struct vkDescriptorPoolHolder;
struct vkPipelineLayoutHolder;
struct rhiTextureBindlessDesc;
class vkImageViewCache;
class rhiSampler;
class rhiSemaphore;
class rhiFence;
class rhiTextureBindlessTable;

class vkDeviceContext final : public rhiDeviceContext
{
public:
	virtual ~vkDeviceContext();

	std::unique_ptr<rhiCommandList> create_commandlist(u32 queue_family) override;
	std::unique_ptr<rhiTextureBindlessTable> create_bindless_table(const rhiTextureBindlessDesc& desc, const u32 set_index = 0) override;
	std::unique_ptr<rhiBuffer> create_buffer(const rhiBufferDesc& desc) override;
	std::unique_ptr<rhiTexture> create_texture(const rhiTextureDesc& desc) override;
	std::unique_ptr<rhiTexture> create_texture_from_path(std::string_view path, bool is_hdr = false, bool srgb = true) override;
	std::shared_ptr<rhiTextureCubeMap> create_texture_cubemap(const rhiTextureDesc& desc) override;
	std::unique_ptr<rhiSampler> create_sampler(const rhiSamplerDesc& desc) override;
	std::unique_ptr<rhiSemaphore> create_semaphore() override;
	std::unique_ptr<rhiFence> create_fence(bool signaled) override;

	rhiDescriptorSetLayout create_descriptor_set_layout(const std::vector<rhiDescriptorSetLayoutBinding>& bindings, u32 set_index = 0);
	rhiDescriptorPool create_descriptor_pool(const rhiDescriptorPoolCreateInfo& create_info, u32 max_sets);
	std::vector<rhiDescriptorSet> allocate_descriptor_sets(rhiDescriptorPool pool, const std::vector<rhiDescriptorSetLayout>& layouts);
	std::vector<rhiDescriptorSet> allocate_descriptor_indexing_sets(rhiDescriptorPool pool, const std::vector<rhiDescriptorSetLayout>& layouts, const std::vector<u32>& counts);
	void update_descriptors(const std::vector<rhiWriteDescriptor>& writes) override;
	rhiDescriptorSetLayout create_descriptor_indexing_set_layout(const rhiDescriptorIndexing& desc, const u32 set_index) override;

	rhiPipelineLayout create_pipeline_layout(std::vector<rhiDescriptorSetLayout> set_layouts, std::vector<rhiPushConstant> push_constant_bytes = {}, void** keep_alive_out = nullptr) override;
	std::unique_ptr<rhiPipeline> create_graphics_pipeline(const rhiGraphicsPipelineDesc& desc, const rhiPipelineLayout& layout) override;
	std::unique_ptr<rhiPipeline> create_compute_pipeline(const rhiComputePipelineDesc& desc, const rhiPipelineLayout& layout) override;
	
	std::shared_ptr<rhiCommandList> begin_onetime_commands(rhiQueueType t = rhiQueueType::graphics) override;
	void submit_and_wait(std::shared_ptr<rhiCommandList> cmd, rhiQueueType t = rhiQueueType::graphics) override;
	void submit(rhiQueueType type, const rhiSubmitInfo& info) override;
	void wait(class rhiFence* f) override;
	void reset(class rhiFence* f) override;

	bool verify_device() const;
	bool verify_phys_device() const;

	void create_imageview_cache();
	void create_vma_allocator(VkInstance instance);
	void create_queue(const std::unordered_map<rhiQueueType, u32>& queue_families);
	std::weak_ptr<vkImageViewCache> get_imageview_cache() const { return imageview_cache; }

public:
	VkDevice device = VK_NULL_HANDLE;
	VkPhysicalDevice phys_device = VK_NULL_HANDLE;
	VmaAllocator allocator = VK_NULL_HANDLE;

private:
	std::vector<std::shared_ptr<vkDescriptorSetLayoutHolder>> kept_descriptor_layouts;
	std::vector<std::shared_ptr<vkDescriptorPoolHolder>> kept_pools;
	std::vector<std::shared_ptr<vkPipelineLayoutHolder>> kept_pipeline_layouts;
	std::shared_ptr<vkImageViewCache> imageview_cache;
};

