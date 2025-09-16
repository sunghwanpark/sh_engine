#pragma once
#include "pch.h"
#include "rhi/rhiDefs.h"
#include "rhi/rhiDescriptor.h"

class rhiTexture;
class rhiSampler;
class rhiDeviceContext;
class rhiFrameContext;
class rhiBuffer;
class rhiCommandList;

struct sharedSamplers
{
    std::unique_ptr<rhiSampler> linear_clamp;
    std::unique_ptr<rhiSampler> linear_wrap;
    std::unique_ptr<rhiSampler> point_clamp;
};

struct descriptorArena
{
public:
    rhiDescriptorPool& current(u32 frame_index) { return pools[frame_index]; }
    std::vector<rhiDescriptorPool> pools;
};

struct alignas(16) globalsCB
{
    mat4 view; // 64
    mat4 proj; // 64
    mat4 inv_view_proj; // 64
    vec4 cam_pos; // 16 (w = padding)
};

struct groupRecord
{
    const rhiBuffer* vbo;
    const rhiBuffer* ibo;
    u32 first_cmd;
    u32 cmd_count;
    u32 base_color_index;
    u32 norm_color_index;
    u32 m_r_color_index;
    u32 base_sam_index;
    u32 norm_sam_index;
    u32 m_r_sam_index;

    f32 alpha_cutoff;
    f32 metalic_factor;
    f32 roughness_factor;
};

class renderShared
{
public:
    ~renderShared();

public:
    void Initialize(rhiDeviceContext* context, rhiFrameContext* frame_context);
    void create_scene_color();

    void create_or_resize_buffer(std::shared_ptr<rhiBuffer>& buffer, const u32 bytes, const rhiBufferUsage usage, const rhiMem mem, const u32 stride);
    void create_or_resize_buffer(std::unique_ptr<rhiBuffer>& buffer, const u32 bytes, const rhiBufferUsage usage, const rhiMem mem, const u32 stride);

    void upload_to_device(rhiCommandList* cmd_list, rhiBuffer* buffer, const void* src, const u32 bytes, const u32 dst_offset = 0);
    void upload_to_device(rhiBuffer* buffer, const void* src, const u32 bytes, const u32 dst_offset = 0);
    const u32 get_frame_size() const;
    void clear_staging_buffer();

private:
    void create_shared_samplers();
    void create_descriptor_pools();

public:
    rhiDeviceContext* context = nullptr;
    rhiFrameContext* frame_context = nullptr;

    sharedSamplers samplers;
    descriptorArena arena;

    std::shared_ptr<rhiTexture> scene_color;
    std::vector<std::unique_ptr<rhiBuffer>> pending_staging_buffers;
};

