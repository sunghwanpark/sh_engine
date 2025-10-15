#pragma once

#include "pch.h"

inline constexpr u64 RHI_WHOLE_SIZE = ~0ull;

struct alignas(16) instanceData 
{
    glm::mat4 model;
    glm::mat4 normal_mat;
};

struct rhiDrawIndexedIndirect 
{
    u32 index_count;        // 4b
    u32 instance_count;     // 4b
    u32 first_index;            // 4b
    i32  vertex_offset;         // 4b
    u32 first_instance;         // 4b
};

enum class rhiQueueType : u8
{
    none = 1u << 0,
    graphics = 1u << 1,
    compute = 1u << 2,
    transfer = 1u << 3,
    present = 1u << 4
};

inline rhiQueueType operator|(rhiQueueType a, rhiQueueType b)
{
    return static_cast<rhiQueueType>(static_cast<u32>(a) | static_cast<u32>(b));
}

inline rhiQueueType operator&(rhiQueueType a, rhiQueueType b)
{
    return static_cast<rhiQueueType>(static_cast<u32>(a) & static_cast<u32>(b));
}
enum class rhiBufferUsage : u32
{
    vertex = 1u << 0,
    index = 1u << 1,
    transfer_src = 1u << 2,
    transfer_dst = 1u << 3,
    storage = 1u << 4,
    uniform = 1u << 5,
    indirect = 1u << 6,
};

inline rhiBufferUsage operator|(rhiBufferUsage a, rhiBufferUsage b) 
{
    return static_cast<rhiBufferUsage>(static_cast<u32>(a) | static_cast<u32>(b));
}

enum class rhiMem : u8
{
    auto_device,
    auto_host,
};

enum class rhiFormat : u32
{
    RGBA8_UNORM,
    RGBA8_SRGB,
    BGRA8_UNORM,
    BGRA8_SRGB,
    RGBA16F,
    RGB32_SFLOAT,
    RGBA32_SFLOAT,
    RG16_SFLOAT,
    RG32_SFLOAT,
    D24S8,
    D32F,
    D32S8
};

enum class rhiLoadOp : u8
{
    load, clear, dont_care
};

enum class rhiStoreOp : u8 
{ 
    none, store, dont_care 
};

enum class rhiSampleCount : u8 
{ 
    x1 = 1, 
    x2 = 2, 
    x4 = 4 
};

enum class rhiResult
{
    ok,
    suboptimal,
    out_of_date
};

enum class rhiPipelineStage : u32 
{ 
    none = 0,
    top_of_pipe = 1u << 0,
    draw_indirect = 1u << 1,
    vertex_input = 1u << 2,
    vertex_shader = 1u << 3,
    fragment_shader = 1u << 4,
    compute_shader = 1u << 5,
    color_attachment_output = 1u << 8,
    transfer = 1u << 9,
    copy = 1u << 10,
    all_graphics = 1u << 11,
    all_commands = 1u << 12,
    early_fragment_test = 1u << 13,
    late_fragment_test = 1u << 14,
    bottom_of_pipe = 1u << 31,
};

inline rhiPipelineStage operator|(rhiPipelineStage a, rhiPipelineStage b)
{
    return static_cast<rhiPipelineStage>(static_cast<u32>(a) | static_cast<u32>(b));
}

enum class rhiShaderStage : u32
{
    vertex = 1u << 0,
    fragment = 1u << 1,
    compute = 1u << 2,
    all = 1u << 3
};

inline rhiShaderStage operator|(rhiShaderStage a, rhiShaderStage b) 
{
    return static_cast<rhiShaderStage>(static_cast<u32>(a) | static_cast<u32>(b));
}

enum class rhiPipelineType 
{ 
    graphics, 
    compute 
};

enum class rhiDescriptorType : u8 
{
    sampler,
    sampled_image,
    storage_image,
    uniform_buffer,
    uniform_buffer_dynamic,
    storage_buffer,
    combined_image_sampler
};

enum class rhiImageLayout : u8 
{ 
    undefined, 
    shader_readonly,
    color_attachment,
    depth_stencil_attachment,
    depth_readonly,
    transfer_src,
    transfer_dst,
    compute,
    present,
    general,
};

enum class rhiFilter : u8 
{
    nearest,
    linear 
};

enum class rhiMipmapMode : u8 
{ 
    nearest, 
    linear 
};

enum class rhiAddressMode : u8 
{ 
    repeat, 
    mirror, 
    clamp_to_edge, 
    clamp_to_border 
};

enum class rhiBorderColor : u8 
{ 
    transparent_black, 
    opaque_black, 
    opaque_white 
};

enum class rhiCompareOp : u8
{
    never,
    less, 
    equal, 
    less_equal,
    greater,
    not_equal,
    greater_equal,
    always
};

enum class rhiImageAspect : u32
{
    color = 1u << 0,
    depth = 1u << 1,
    stencil = 1u << 2, 
    depth_stencil = 1u << 3 
};

enum class rhiAccessFlags : u32
{
    none,
    index_read = 1u << 1,
    vertex_attribute_read = 1u << 2,
    uniform_read = 1u << 3,
    shader_read = 1u << 4,
    shader_write = 1u << 5,
    color_attachment_read = 1u << 6,
    color_attachment_write = 1u << 7,
    depthstencil_attachment_read = 1u << 8,
    depthstencil_attachment_write = 1u << 9,
    transfer_read = 1u << 10,
    transfer_write = 1u << 11,
    host_read = 1u << 12,
    host_write = 1u << 13,
    memory_read = 1u << 14,
    memory_write = 1u << 15,
    shader_storage_read = 1u << 16,
    shader_storage_write = 1u << 17,
    draw_indirect = 1u << 18,
    indirect_command_read = 1u << 19,
    shader_sampled_read = 1u << 20
};
inline rhiAccessFlags operator|(rhiAccessFlags a, rhiAccessFlags b)
{
    return static_cast<rhiAccessFlags>(static_cast<u16>(a) | static_cast<u16>(b));
}

enum class rhiMipsMethod { auto_select, linear_blit, compute };

enum class rhiImageViewType 
{ 
    type_2d, 
    type_2darray
};

enum class rhiCullMode
{
    back,
    front,
    none
};

enum class rhiCubemapViewType : u8 { mip, cube };

enum class rhiDescriptorBindingFlags : u16
{
    update_after_bind = 1u << 0,
    update_unused_while_pending = 1u << 1,
    partially_bound = 1u << 2,
    variable_descriptor_count = 1u << 3,
};
inline rhiDescriptorBindingFlags operator|(rhiDescriptorBindingFlags a, rhiDescriptorBindingFlags b)
{
    return static_cast<rhiDescriptorBindingFlags>(static_cast<u16>(a) | static_cast<u16>(b));
}

enum class rhiDescriptorPoolCreateFlags : u16
{
    free_descriptor_set = 1u << 0,
    update_after_bind = 1u << 1,
    host_only = 1u << 2,
    allow_overallocation_sets = 1u << 3,
    allow_overallocation_pools = 1u << 4
};

enum class rhiTextureUsage : u32
{
    transfer_src = 1u << 0,
    transfer_dst = 1u << 1,
    sampled = 1u << 2,
    storage = 1u << 3,
    color_attachment = 1u << 4,
    depth_stencil = 1u << 5,
    transient_attachment = 1u << 6,
    input_attachment = 1u << 7,

    normal = color_attachment | sampled,
    ds = depth_stencil | sampled,
    from_file = sampled | transfer_dst | transfer_src
};
inline rhiTextureUsage operator|(rhiTextureUsage a, rhiTextureUsage b)
{
    return static_cast<rhiTextureUsage>(static_cast<u32>(a) | static_cast<u32>(b));
}

enum class rhiTextureType
{
    base_color,
    base_color_sampler,
    normal,
    normal_sampler,
    metalic_roughness,
    metalic_roughness_sampler
};

enum class rhiBlendFactor
{
    zero,
    one,
    src_color,
    one_minus_src_color,
    dst_color,
    one_minus_dst_color,
    src_alpha,
    one_minus_src_alpha,
    dst_alpha,
    one_minus_dst_alpha
};

enum class rhiBlendOp
{
    add,
    subtract,
    reverse_subtract,
    min,
    max
};

enum class rhiColorComponentBit : u8
{
    r = 1u << 0,
    g = 1u << 1, 
    b = 1u << 2, 
    a = 1u << 3
};
inline rhiColorComponentBit operator|(rhiColorComponentBit a, rhiColorComponentBit b)
{
    return static_cast<rhiColorComponentBit>(static_cast<u8>(a) | static_cast<u8>(b));
}