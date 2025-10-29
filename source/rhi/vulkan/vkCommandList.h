#pragma once

#include "pch.h"
#include "rhi/rhiCommandList.h"

class vkDeviceContext;
class vkImageViewCache;
class vkPipeline;
class vkCommandList final : public rhiCommandList
{
public:
    vkCommandList(vkDeviceContext* context, VkCommandPool pool, VkCommandBuffer cmd_buffer, bool is_transient);
    virtual ~vkCommandList();

public:
    void begin(u32 flags = 0) override;
    void end() override;
    void reset() override;
    void begin_render_pass(const rhiRenderingInfo& info) override;
    void end_render_pass() override;
    void set_viewport_scissor(const vec2 vp_size) override;

    void copy_buffer(rhiBuffer* src, const u32 src_offset, rhiBuffer* dst, const u32 dst_offset, const u64 bytes) override;
    void copy_buffer_to_image(rhiBuffer* src_buf, rhiTexture* dst_tex, rhiImageLayout layout, std::span<const rhiBufferImageCopy> regions) override;
    void image_barrier(rhiTexture* tex, rhiImageLayout old_layout, rhiImageLayout new_layout, u32 base_mip = 0, u32 level_count = 1, u32 base_layer = 0, u32 layer_count = 1, bool is_same_stage = false) override;
    void image_barrier(rhiTextureCubeMap* tex, rhiImageLayout old_layout, rhiImageLayout new_layout, u32 base_mip = 0, u32 level_count = 1, u32 base_layer = 0, u32 layer_count = 1, bool is_same_stage = false) override;
    void image_barrier(rhiTexture* tex, const rhiImageBarrierDescription& desc) override;
    void buffer_barrier(rhiBuffer* buf, const rhiBufferBarrierDescription& desc) override;

    void generate_mips(rhiTexture* tex, const rhiGenMipsDesc& desc) override;

    void bind_pipeline(rhiPipeline* p) override;
    void bind_vertex_buffer(rhiBuffer* vbo, const u32 slot, const u32 offset) override;
    void bind_index_buffer(rhiBuffer* ibo, const u32 offset) override;
    void bind_descriptor_sets(rhiPipelineLayout layout, rhiPipelineType pipeline, const std::vector<rhiDescriptorSet>& sets, const u32 first_set, const std::vector<u32>& dynamic_offsets) override;

    void push_constants(const rhiPipelineLayout layout, rhiShaderStage stages, u32 offset, u32 size, const void* data) override;
    void set_cullmode(const rhiCullMode cull_mode) override;

    void draw_indexed_indirect(rhiBuffer* indirect_buffer, const u32 offset, const u32 draw_count, const u32 stride) override;
    void draw_mesh_tasks_indirect(rhiBuffer* indirect_buffer, const u32 offset, const u32 draw_count, const u32 stride) override;
    void draw_fullscreen() override;

    void dispatch(const u32 x, const u32 y, const u32 z) override;

    VkCommandBuffer get_cmd_buffer() const { return cmd_buffer; }
    VkCommandPool get_cmd_pool() const { return cmd_pool; }
    bool is_transient_() const { return is_transient; }

private:
    void allocate();
    void generate_mips_compute(rhiTexture* tex, const rhiGenMipsDesc& desc);

private:
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice phys_device = VK_NULL_HANDLE;
    std::weak_ptr<vkImageViewCache> weak_imgview_cache;
    VkCommandBuffer cmd_buffer;
    VkCommandPool cmd_pool;
    bool is_transient = false;
};