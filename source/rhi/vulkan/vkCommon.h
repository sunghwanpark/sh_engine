#pragma once
#include "pch.h"
#include "rhi/rhiDefs.h"
#include "rhi/rhiTextureView.h"
#include "rhi/rhiDescriptor.h"
#include "vkTexture.h"
#include "vkSampler.h"
#include "vkPipeline.h"

inline const char* vkToString(VkResult vkResult) 
{
    switch (vkResult) 
    {
    case VK_SUCCESS:
        return "VK_SUCCESS";
    case VK_NOT_READY:
        return "VK_NOT_READY";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    case VK_EVENT_SET:
        return "VK_EVENT_SET";
    case VK_EVENT_RESET:
        return "VK_EVENT_RESET";
    case VK_INCOMPLETE:
        return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:
        return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN:
        return "VK_ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_FRAGMENTATION:
        return "VK_ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
        return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_PIPELINE_COMPILE_REQUIRED:
        return "VK_PIPELINE_COMPILE_REQUIRED";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:
        return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV:
        return "VK_ERROR_INVALID_SHADER_NV";
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
        return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
    case VK_ERROR_NOT_PERMITTED_KHR:
        return "VK_ERROR_NOT_PERMITTED_KHR";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
    case VK_THREAD_IDLE_KHR:
        return "VK_THREAD_IDLE_KHR";
    case VK_THREAD_DONE_KHR:
        return "VK_THREAD_DONE_KHR";
    case VK_OPERATION_DEFERRED_KHR:
        return "VK_OPERATION_DEFERRED_KHR";
    case VK_OPERATION_NOT_DEFERRED_KHR:
        return "VK_OPERATION_NOT_DEFERRED_KHR";
    default:
        return "Unhandled VkResult";
    }
}

#define VK_CHECK_ERROR(call)                                                       \
    do {                                                                           \
        VkResult _vk_result = (call);                                              \
        if (_vk_result != VK_SUCCESS) {                                            \
            throw std::runtime_error(                                              \
                std::string("Vulkan error: ") + vkToString(_vk_result) +     \
                " | expr: " + std::string(#call) +                                 \
                " | file: " + std::string(__FILE__) +                              \
                " | line: " + std::to_string(__LINE__)                             \
            );                                                                     \
        }                                                                          \
    } while (0)

#define VK_HANDLE_CHECK(h)                                              \
    do {                                                                \
        if ((h) == VK_NULL_HANDLE) {                                    \
            throw std::runtime_error("Vulkan handle is VK_NULL_HANDLE at " + std::string(__FILE__) + ":" + std::to_string(__LINE__)); \
        }                                                               \
    } while (0)

inline VkSampler get_vk_sampler(rhiSampler* s)
{
    if (!s)
        return VK_NULL_HANDLE;

    auto* vks = dynamic_cast<vkSampler*>(s);
    assert(vks);

    return vks->get_sampler();
}

inline VkPipelineLayout get_vk_pipeline_layout(const rhiPipelineLayout& layout)
{
    auto vk_layout = reinterpret_cast<VkPipelineLayout>(layout.native);
    assert(vk_layout != VK_NULL_HANDLE);
    return vk_layout;
}

inline VkDescriptorSet get_vk_descriptor_set(const rhiDescriptorSet& set)
{
    return reinterpret_cast<VkDescriptorSet>(set.native);
}

inline VkFormat vk_format(rhiFormat f)
{
    switch (f)
    {
    case rhiFormat::RGBA8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
    case rhiFormat::BGRA8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
    case rhiFormat::RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
    case rhiFormat::RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case rhiFormat::BGRA8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
    case rhiFormat::RGB32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
    case rhiFormat::RGBA32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case rhiFormat::RG32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT;
    case rhiFormat::D24S8: return VK_FORMAT_D24_UNORM_S8_UINT;
    case rhiFormat::D32F: return VK_FORMAT_D32_SFLOAT;
    case rhiFormat::D32S8: return VK_FORMAT_D32_SFLOAT_S8_UINT;
    }
    return VK_FORMAT_R8G8B8A8_UNORM;
}

inline VkImageLayout vk_layout(rhiImageLayout l)
{
    switch (l) 
    {
    case rhiImageLayout::undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
    case rhiImageLayout::shader_readonly: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case rhiImageLayout::color_attachment: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case rhiImageLayout::depth_stencil_attachment: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case rhiImageLayout::depth_readonly: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    case rhiImageLayout::transfer_src: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case rhiImageLayout::transfer_dst: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case rhiImageLayout::present: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case rhiImageLayout::general:return VK_IMAGE_LAYOUT_GENERAL;
    }
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

inline VkImageAspectFlags vk_aspect(rhiImageAspect aspect)
{
    switch (aspect)
    {
    case rhiImageAspect::color: return VK_IMAGE_ASPECT_COLOR_BIT;
    case rhiImageAspect::depth: return VK_IMAGE_ASPECT_DEPTH_BIT;
    case rhiImageAspect::stencil: return VK_IMAGE_ASPECT_STENCIL_BIT;
    case rhiImageAspect::depth_stencil: return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

inline VkImageAspectFlags vk_aspect_from_format(rhiFormat fmt)
{
    switch (fmt) 
    {
    case rhiFormat::D32F:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case rhiFormat::D32S8:
    case rhiFormat::D24S8:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

inline VkSampleCountFlagBits vk_sample(rhiSampleCount s)
{
    switch (s)
    {
    case rhiSampleCount::x1: return VK_SAMPLE_COUNT_1_BIT;
    case rhiSampleCount::x2: return VK_SAMPLE_COUNT_2_BIT;
    case rhiSampleCount::x4: return VK_SAMPLE_COUNT_4_BIT;
    }
    return VK_SAMPLE_COUNT_1_BIT;
}

inline VkFilter vk_filter(rhiFilter f)
{
    return f == rhiFilter::nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

inline VkSamplerMipmapMode vk_mipmap(rhiMipmapMode m)
{
    return m == rhiMipmapMode::nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

inline VkSamplerAddressMode vk_address_mode(rhiAddressMode m)
{
    switch (m)
    {
    case rhiAddressMode::repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case rhiAddressMode::mirror: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case rhiAddressMode::clamp_to_edge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case rhiAddressMode::clamp_to_border: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    }
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
}

inline VkBorderColor vk_border_color(rhiBorderColor c)
{
    switch (c)
    {
    case rhiBorderColor::opaque_white: return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    case rhiBorderColor::opaque_black: return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    case rhiBorderColor::transparent_black: return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    }
    return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
}

inline VkCompareOp vk_compare_op(rhiCompareOp op)
{
    switch (op)
    {
    case rhiCompareOp::never: return VK_COMPARE_OP_NEVER;
    case rhiCompareOp::less: return VK_COMPARE_OP_LESS;
    case rhiCompareOp::equal: return VK_COMPARE_OP_EQUAL;
    case rhiCompareOp::less_equal: return VK_COMPARE_OP_LESS_OR_EQUAL;
    case rhiCompareOp::greater: return VK_COMPARE_OP_GREATER;
    case rhiCompareOp::not_equal: return VK_COMPARE_OP_NOT_EQUAL;
    case rhiCompareOp::greater_equal: return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case rhiCompareOp::always: return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_LESS_OR_EQUAL;
}

inline VkDescriptorType vk_desc_type(rhiDescriptorType t)
{
    switch (t)
    {
    case rhiDescriptorType::sampler:               return VK_DESCRIPTOR_TYPE_SAMPLER;
    case rhiDescriptorType::sampled_image:          return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case rhiDescriptorType::combined_image_sampler:  return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case rhiDescriptorType::storage_image:          return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case rhiDescriptorType::uniform_buffer:         return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case rhiDescriptorType::uniform_buffer_dynamic:         return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    case rhiDescriptorType::storage_buffer:         return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

inline VkAttachmentLoadOp vk_load_op(rhiLoadOp op)
{
    switch (op)
    {
    case rhiLoadOp::load: return VK_ATTACHMENT_LOAD_OP_LOAD;
    case rhiLoadOp::clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case rhiLoadOp::dont_care: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

inline VkAttachmentStoreOp vk_store_op(rhiStoreOp op)
{
    switch (op)
    {
    case rhiStoreOp::store: return VK_ATTACHMENT_STORE_OP_STORE;
    case rhiStoreOp::dont_care: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

inline VkPipelineBindPoint vk_bind_point(rhiPipelineType t)
{
    switch (t)
    {
    case rhiPipelineType::graphics: return VK_PIPELINE_BIND_POINT_GRAPHICS;
    case rhiPipelineType::compute: return VK_PIPELINE_BIND_POINT_COMPUTE;
    }
    return VK_PIPELINE_BIND_POINT_GRAPHICS;
}

template<class T>
inline bool has_(const T& origin, const T& comp)
{
    return (static_cast<u32>(origin) & static_cast<u32>(comp)) != 0;
};

inline VkBufferUsageFlags get_vk_buffer_usage(const rhiBufferUsage& usage)
{
    VkBufferUsageFlags u = 0;
    if (has_<rhiBufferUsage>(usage, rhiBufferUsage::transfer_src))
        u |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (has_<rhiBufferUsage>(usage, rhiBufferUsage::transfer_dst))
        u |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (has_<rhiBufferUsage>(usage, rhiBufferUsage::vertex))
        u |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (has_<rhiBufferUsage>(usage, rhiBufferUsage::index))
        u |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (has_<rhiBufferUsage>(usage, rhiBufferUsage::uniform))
        u |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (has_<rhiBufferUsage>(usage, rhiBufferUsage::storage))
        u |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (has_<rhiBufferUsage>(usage, rhiBufferUsage::indirect))
        u |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    return u;
}

inline VkPipelineStageFlags vk_pipeline_stage(rhiPipelineStage stage)
{
    switch (stage)
    {
    case rhiPipelineStage::top_of_pipe: return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    case rhiPipelineStage::draw_indirect: return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    case rhiPipelineStage::vertex_input: return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    case rhiPipelineStage::vertex_shader: return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    case rhiPipelineStage::fragment_shader: return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    case rhiPipelineStage::color_attachment_output: return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case rhiPipelineStage::transfer: return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case rhiPipelineStage::bottom_of_pipe: return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
}

inline VkPipelineStageFlags2 vk_pipeline_stage2(rhiPipelineStage stage)
{
    switch (stage)
    {
    case rhiPipelineStage::top_of_pipe: return VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    case rhiPipelineStage::draw_indirect: return VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    case rhiPipelineStage::vertex_input: return VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
    case rhiPipelineStage::vertex_shader: return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    case rhiPipelineStage::fragment_shader: return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    case rhiPipelineStage::color_attachment_output: return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    case rhiPipelineStage::transfer: return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    case rhiPipelineStage::copy: return VK_PIPELINE_STAGE_2_COPY_BIT;
    case rhiPipelineStage::bottom_of_pipe: return VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    }
    return VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
}

inline VkAccessFlags2 vk_access_flags2(rhiAccessFlags f)
{
    VkAccessFlags2 vk_flags = VK_ACCESS_NONE;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::indirect_command_read))
        vk_flags |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::index_read))
        vk_flags |= VK_ACCESS_2_INDEX_READ_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::vertex_attribute_read))
        vk_flags |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::uniform_read))
        vk_flags |= VK_ACCESS_2_UNIFORM_READ_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::shader_read))
        vk_flags |= VK_ACCESS_2_SHADER_READ_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::shader_write))
        vk_flags |= VK_ACCESS_2_SHADER_WRITE_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::color_attachment_read))
        vk_flags |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::color_attachment_write))
        vk_flags |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::depthstencil_attachment_read))
        vk_flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::depthstencil_attachment_write))
        vk_flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::transfer_read))
        vk_flags |= VK_ACCESS_2_TRANSFER_READ_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::transfer_write))
        vk_flags |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::host_read))
        vk_flags |= VK_ACCESS_2_HOST_READ_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::host_write))
        vk_flags |= VK_ACCESS_2_HOST_WRITE_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::memory_read))
        vk_flags |= VK_ACCESS_2_MEMORY_READ_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::memory_write))
        vk_flags |= VK_ACCESS_2_MEMORY_WRITE_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::shader_storage_read))
        vk_flags |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
    if (has_<rhiAccessFlags>(f, rhiAccessFlags::shader_storage_write))
        vk_flags |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

    return vk_flags;
}