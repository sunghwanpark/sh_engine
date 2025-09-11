#pragma once

#include "pch.h"
#include "rhi/rhiDefs.h"

class rhiBuffer;
class rhiPipeline;
class rhiTexture;
struct rhiRenderingInfo;
struct rhiPipelineLayout;
struct rhiDescriptorSet;
struct rhiBufferImageCopy;
struct rhiGenMipsDesc;
class rhiCommandList 
{
public:
    virtual ~rhiCommandList() = default;
    virtual void begin(u32 flags = 0) = 0;
    virtual void end() = 0;
    virtual void reset() = 0;
    virtual void begin_render_pass(const rhiRenderingInfo& info) = 0;
    virtual void end_render_pass() = 0;
    virtual void set_viewport_scissor(const vec2 vp_size) = 0;

    virtual void copy_buffer(rhiBuffer* src, const u32 src_offset, rhiBuffer* dst, const u32 dst_offset, const u64 bytes) = 0;
    virtual void copy_buffer_to_image(rhiBuffer* src_buf, rhiTexture* dst_tex, rhiImageLayout layout, std::span<const rhiBufferImageCopy> regions) = 0;
    virtual void image_barrier(rhiTexture* tex, rhiImageLayout old_layout, rhiImageLayout new_layout, u32 base_mip = 0, u32 level_count = 1, u32 base_layer = 0, u32 layer_count = 1, bool is_same_stage = false) = 0;
    virtual void buffer_barrier(rhiBuffer* buf, rhiPipelineStage src_stage, rhiPipelineStage dst_stage, rhiAccessFlags src_access, rhiAccessFlags dst_access, u32 offset, u64 size) = 0;

    virtual void generate_mips(rhiTexture* tex, const rhiGenMipsDesc& desc) = 0;

    virtual void bind_pipeline(rhiPipeline* p) = 0;
    virtual void bind_vertex_buffer(rhiBuffer* vbo, const u32 slot, const u32 offset) = 0;
    virtual void bind_index_buffer(rhiBuffer* ibo, const u32 offset) = 0;
    virtual void bind_descriptor_sets(rhiPipelineLayout layout, rhiPipelineType pipeline, const std::vector<rhiDescriptorSet>& sets, const u32 first_set, const std::vector<u32>& dynamic_offsets) = 0;
    
    virtual void push_constants(const rhiPipelineLayout layout, rhiShaderStage stages, u32 offset, u32 size, const void* data) = 0;

    virtual void draw_indexed_indirect(rhiBuffer* indirect_buffer, const u32 offset, const u32 draw_count, const u32 stride) = 0;
    virtual void draw_fullscreen() = 0;

    virtual void dispatch(const u32 x, const u32 y, const u32 z) = 0;
};