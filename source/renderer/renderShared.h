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
struct rhiImageBarrierDescription;
struct rhiBufferBarrierDescription;

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
    mat4 view_proj; // 64
    vec4 cam_pos; // 16 (w = padding)
};

enum class drawType : u8
{
    gbuffer = 0,
    translucent = 1,
    count
};
constexpr u32 draw_type_count = static_cast<u32>(drawType::count);
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

struct alignas(16) materialPC
{
    u32 base_color_index;
    u32 norm_color_index;
    u32 mr_color_index;
    u32 base_sampler_index;
    u32 norm_sampler_index;
    u32 mr_sampler_index;

    f32 alpha_cutoff;
    f32 metalic_factor;
    f32 roughness_factor;
    f32 __pad[3];

    materialPC(const groupRecord& g)
        : base_color_index(g.base_color_index),
        norm_color_index(g.norm_color_index),
        mr_color_index(g.m_r_color_index),
        base_sampler_index(g.base_sam_index),
        norm_sampler_index(g.norm_sam_index),
        mr_sampler_index(g.m_r_sam_index),
        alpha_cutoff(g.alpha_cutoff),
        metalic_factor(g.metalic_factor),
        roughness_factor(g.roughness_factor) {}
};

using groupRecordArray = std::array<std::vector<groupRecord>, draw_type_count>;
using indirectArray = std::array<std::vector<rhiDrawIndexedIndirect>, draw_type_count>;
using instanceArray = std::array<std::vector<instanceData>, draw_type_count>;
using drawTypeBuffers = std::array<std::shared_ptr<rhiBuffer>, draw_type_count>;

class renderShared
{
public:
    ~renderShared();

public:
    void Initialize(rhiDeviceContext* context, rhiFrameContext* frame_context);
    void create_scene_color();

    void create_or_resize_buffer(std::shared_ptr<rhiBuffer>& buffer, const u32 bytes, const rhiBufferUsage usage, const rhiMem mem, const u32 stride);
    void create_or_resize_buffer(std::unique_ptr<rhiBuffer>& buffer, const u32 bytes, const rhiBufferUsage usage, const rhiMem mem, const u32 stride);

    void image_barrier(rhiTexture* texture, const rhiImageBarrierDescription& desc);
    void buffer_barrier(rhiBuffer* buffer, const rhiBufferBarrierDescription& desc);
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

