#include "vkDeviceContext.h"
#include "vkCommon.h"
#include "vkQueue.h"
#include "vkTexture.h"
#include "vkTextureCubemap.h"
#include "vkBuffer.h"
#include "vkSampler.h"
#include "vkSynchronize.h"
#include "vkDescriptor.h"
#include "vkPipeline.h"
#include "vkCommandList.h"
#include "vkImageViewCache.h"
#include "vkBindlessTable.h"
#include "rhi/rhiDescriptor.h"
#include "rhi/rhiSubmitInfo.h"

namespace
{
    rhiPipelineLayout vk_create_pipeline_layout(VkDevice device, const std::vector<rhiDescriptorSetLayout>& set_layouts, const std::vector<rhiPushConstant>& push_constant_bytes, std::shared_ptr<vkPipelineLayoutHolder>& keep_alive)
    {
        keep_alive = std::make_shared<vkPipelineLayoutHolder>(device);

        std::vector<VkDescriptorSetLayout> layouts;
        layouts.reserve(set_layouts.size());
        for (auto& s : set_layouts)
        {
            layouts.push_back(reinterpret_cast<VkDescriptorSetLayout>(s.native));
        }

        VkPipelineLayoutCreateInfo create_info
        { 
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = static_cast<u32>(layouts.size()),
            .pSetLayouts = layouts.data()
        };

        std::vector<VkPushConstantRange> push_constants;
        if (push_constant_bytes.size() > 0)
        {
            push_constants.resize(push_constant_bytes.size());
            u32 offset = 0;
            for(u32 index = 0; index < push_constants.size(); ++index)
            {
                push_constants[index].stageFlags = vk_shader_stage(push_constant_bytes[index].stage);
                push_constants[index].offset = offset;
                push_constants[index].size = push_constant_bytes[index].bytes;
                offset += push_constant_bytes[index].bytes;
            }
            create_info.pushConstantRangeCount = static_cast<u32>(push_constants.size());
            create_info.pPushConstantRanges = push_constants.data();
        }
        VK_CHECK_ERROR(vkCreatePipelineLayout(device, &create_info, nullptr, &keep_alive->layout));
        return { .native = keep_alive->layout };
    }

    static VkShaderModule vk_create_shader(VkDevice dev, const rhiShaderBinary& bin) 
    {
        const VkShaderModuleCreateInfo create_info
        {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = bin.size,
            .pCode = reinterpret_cast<const uint32_t*>(bin.data)
        };
        VkShaderModule m = VK_NULL_HANDLE;
        VK_CHECK_ERROR(vkCreateShaderModule(dev, &create_info, nullptr, &m));
        return m;
    }

    std::unique_ptr<rhiPipeline> vk_create_graphics_pipeline(VkDevice device, const rhiGraphicsPipelineDesc& desc, const rhiPipelineLayout& layout)
    {
        auto p = std::make_unique<vkPipeline>(device, desc, rhiPipelineType::graphics);

        std::vector<VkPipelineShaderStageCreateInfo> stages;
        // vs
        VkShaderModule vs = VK_NULL_HANDLE, fs = VK_NULL_HANDLE;
        if (desc.vs.has_value())
        {
            vs = vk_create_shader(device, desc.vs.value());
            stages.push_back(VkPipelineShaderStageCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_VERTEX_BIT,
                    .module = vs,
                    .pName = desc.vs.value().entry.length() > 0 ? desc.vs.value().entry.c_str() : "main"
                });
        }

        // ps
        if (desc.fs.has_value())
        {
            fs = vk_create_shader(device, desc.fs.value());
            stages.push_back(VkPipelineShaderStageCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .module = fs,
                    .pName = desc.fs.value().entry.length() > 0 ? desc.fs.value().entry.c_str() : "main"
                });
        }

        const VkVertexInputBindingDescription binding{
            .binding = desc.vertex_layout.binding,
            .stride = desc.vertex_layout.stride,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };

        std::vector<VkVertexInputAttributeDescription> vertex_attr_desc;
        vertex_attr_desc.reserve(desc.vertex_layout.vertex_attr_desc.size());
        for (const auto& desc : desc.vertex_layout.vertex_attr_desc)
        {
            vertex_attr_desc.push_back(VkVertexInputAttributeDescription{
                .location = desc.location,
                .binding = desc.binding,
                .format = vk_format(desc.format),
                .offset = desc.offset
                });
        }

        const VkPipelineVertexInputStateCreateInfo vertex_input{ 
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &binding,
            .vertexAttributeDescriptionCount = static_cast<u32>(vertex_attr_desc.size()),
            .pVertexAttributeDescriptions = vertex_attr_desc.data()
        };
        const VkPipelineInputAssemblyStateCreateInfo input_assembly{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
        };
        const VkPipelineViewportStateCreateInfo viewport_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1
        };
        const VkPipelineRasterizationStateCreateInfo raster_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            // rasterizerDiscardEnable가 VK_TRUE면 VkPipelineRenderingCreateInfo의 depth format이 늘 UNDEFINED가 되어버린다.
            .rasterizerDiscardEnable = desc.fs.has_value() ? VK_FALSE : VK_TRUE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.f,
        };
        const VkPipelineMultisampleStateCreateInfo multisample{
            .sType =VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = vk_sample(desc.samples)
        };
        const VkPipelineDepthStencilStateCreateInfo ds_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = desc.depth_test ? VK_TRUE : VK_FALSE,
            .depthWriteEnable = desc.depth_write ? VK_TRUE : VK_FALSE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
        };
        
        std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments;
        color_blend_attachments.resize(desc.color_formats.size());
        for (u32 index = 0; index < desc.color_formats.size(); ++index)
        {
            color_blend_attachments[index].colorWriteMask = 0xF;
            color_blend_attachments[index].blendEnable = VK_FALSE;
            if (desc.blend_states.size() > index)
            {
                color_blend_attachments[index].blendEnable = VK_TRUE;
                color_blend_attachments[index].srcColorBlendFactor = vk_blend_factor(desc.blend_states[index].src_color);
                color_blend_attachments[index].dstColorBlendFactor = vk_blend_factor(desc.blend_states[index].dst_color);
                color_blend_attachments[index].colorBlendOp = vk_blend_op(desc.blend_states[index].color_blend);
                color_blend_attachments[index].srcAlphaBlendFactor = vk_blend_factor(desc.blend_states[index].src_alpha);
                color_blend_attachments[index].dstAlphaBlendFactor = vk_blend_factor(desc.blend_states[index].dst_alpha);
                color_blend_attachments[index].alphaBlendOp = vk_blend_op(desc.blend_states[index].alpha_blend);
                color_blend_attachments[index].colorWriteMask = vk_color_component_bit(desc.blend_states[index].color_write_mask);
            }
        }
        const VkPipelineColorBlendStateCreateInfo color_blend_state{ 
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, 
            .attachmentCount = static_cast<u32>(color_blend_attachments.size()),
            .pAttachments = color_blend_attachments.data()
        };

        VkPipelineDynamicStateCreateInfo dyn_state{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        if (desc.use_dynamic_cullmode)
        {
            const std::array<VkDynamicState, 3> dyn_states = { VK_DYNAMIC_STATE_CULL_MODE_EXT, VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
            dyn_state.dynamicStateCount = 3;
            dyn_state.pDynamicStates = dyn_states.data();
        }
        else
        {
            const std::array<VkDynamicState, 2> dyn_states = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
            dyn_state.dynamicStateCount = 2;
            dyn_state.pDynamicStates = dyn_states.data();
        }

        // dynamic rendering - pipeline format hint
        std::vector<VkFormat> color_formats;
        color_formats.reserve(desc.color_formats.size());
        for (const auto f : desc.color_formats)
        {
            color_formats.push_back(vk_format(f));
        }
        VkPipelineRenderingCreateInfo render_create_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = static_cast<u32>(color_formats.size()),
            .pColorAttachmentFormats = color_formats.size() > 0 ? color_formats.data() : nullptr
        };
        if (desc.depth_format.has_value())
            render_create_info.depthAttachmentFormat = vk_format(desc.depth_format.value());

        const VkGraphicsPipelineCreateInfo pipeline_create_info{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &render_create_info,
            .stageCount = static_cast<u32>(stages.size()),
            .pStages = stages.data(),
            .pVertexInputState = &vertex_input,
            .pInputAssemblyState = &input_assembly,
            .pViewportState = &viewport_state,
            .pRasterizationState = &raster_state,
            .pMultisampleState = &multisample,
            .pDepthStencilState = &ds_state,
            .pColorBlendState = &color_blend_state,
            .pDynamicState = &dyn_state,
            .layout = reinterpret_cast<VkPipelineLayout>(layout.native),
        };
        // renderPass=nullptr, subpass=0  (Dynamic Rendering)

        VK_CHECK_ERROR(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &p->handle));
        if(vs != VK_NULL_HANDLE)
            vkDestroyShaderModule(device, vs, nullptr);
        if(fs != VK_NULL_HANDLE)
            vkDestroyShaderModule(device, fs, nullptr);

        return p;
    }

    std::unique_ptr<rhiPipeline> vk_create_compute_pipeline(VkDevice device, const rhiComputePipelineDesc& desc, const rhiPipelineLayout& layout)
    {
        auto p = std::make_unique<vkPipeline>(device, desc, rhiPipelineType::compute);
        VkShaderModule cs = vk_create_shader(device, desc.cs);

        const VkPipelineShaderStageCreateInfo pipeline_stage_ci{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = cs,
            .pName = desc.cs.entry.length() > 0 ? desc.cs.entry.c_str() : "main",
        };
        const VkComputePipelineCreateInfo compute_ci{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = pipeline_stage_ci,
            .layout = reinterpret_cast<VkPipelineLayout>(layout.native)
        };

        VK_CHECK_ERROR(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_ci, nullptr, &p->handle));
        vkDestroyShaderModule(device, cs, nullptr);
        return p;
    }
}

vkDeviceContext::~vkDeviceContext()
{
    imageview_cache->clear();
    imageview_cache.reset();
    if (device != VK_NULL_HANDLE)
        vkDestroyDevice(device, nullptr);
}

std::unique_ptr<rhiCommandList> vkDeviceContext::create_commandlist(u32 queue_family)
{
    VkCommandPoolCreateInfo create_info
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = queue_family
    };
    VkCommandPool pool;
    VK_CHECK_ERROR(vkCreateCommandPool(device, &create_info, nullptr, &pool));

    VkCommandBufferAllocateInfo allocate_info
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer buffer;
    VK_CHECK_ERROR(vkAllocateCommandBuffers(device, &allocate_info, &buffer));

    return std::make_unique<vkCommandList>(this, pool, buffer, false);
}

bool vkDeviceContext::verify_device() const
{
    return device != VK_NULL_HANDLE;
}

bool vkDeviceContext::verify_phys_device() const
{
    return phys_device != VK_NULL_HANDLE;
}

std::unique_ptr<rhiBindlessTable> vkDeviceContext::create_bindless_table(const rhiBindlessDesc& desc, const u32 set_index)
{
    return std::make_unique<vkBindlessTable>(this, desc, set_index);
}

std::unique_ptr<rhiBuffer> vkDeviceContext::create_buffer(const rhiBufferDesc& desc)
{
    return std::make_unique<vkBuffer>(this, desc);
}

std::unique_ptr<rhiTexture> vkDeviceContext::create_texture(const rhiTextureDesc& desc)
{
    return std::make_unique<vkTexture>(this, desc);
}

std::unique_ptr<rhiTexture> vkDeviceContext::create_texture_from_path(std::string_view path, bool is_hdr, bool srgb)
{
    return std::make_unique<vkTexture>(this, path, is_hdr, srgb);
}

std::shared_ptr<rhiTextureCubeMap> vkDeviceContext::create_texture_cubemap(const rhiTextureDesc& desc)
{
    return std::make_unique<vkTextureCubemap>(this, desc);
}

std::unique_ptr<rhiSampler> vkDeviceContext::create_sampler(const rhiSamplerDesc& desc)
{
    return std::make_unique<vkSampler>(this, desc);
}

std::unique_ptr<rhiSemaphore> vkDeviceContext::create_semaphore()
{
    return std::make_unique<vkSemaphore>(device);
}

std::unique_ptr<rhiFence> vkDeviceContext::create_fence(bool signaled)
{
    return std::make_unique<vkFence>(device, signaled);
}

rhiDescriptorSetLayout vkDeviceContext::create_descriptor_set_layout(const std::vector<rhiDescriptorSetLayoutBinding>& bindings, u32 set_index)
{
    std::vector<VkDescriptorSetLayoutBinding> vk_bindings;
    vk_bindings.reserve(bindings.size());

    std::ranges::for_each(bindings, [&](const rhiDescriptorSetLayoutBinding& binding)
        {
            VkShaderStageFlags stage{};
            if (static_cast<u32>(binding.stage) & static_cast<u32>(rhiShaderStage::vertex))
                stage |= VK_SHADER_STAGE_VERTEX_BIT;
            if (static_cast<u32>(binding.stage) & static_cast<u32>(rhiShaderStage::fragment))
                stage |= VK_SHADER_STAGE_FRAGMENT_BIT;
            if (static_cast<u32>(binding.stage) & static_cast<u32>(rhiShaderStage::compute))
                stage |= VK_SHADER_STAGE_COMPUTE_BIT;

            const VkDescriptorSetLayoutBinding layout_binding
            {
                .binding = binding.binding,
                .descriptorType = vk_desc_type(binding.type),
                .descriptorCount = binding.count,
                .stageFlags = stage
            };
            vk_bindings.push_back(layout_binding);
        });

    auto holder = std::make_shared<vkDescriptorSetLayoutHolder>(device);
    const VkDescriptorSetLayoutCreateInfo create_info
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<u32>(vk_bindings.size()),
        .pBindings = vk_bindings.data()
    };
    holder->bindings = std::move(vk_bindings);
    VK_CHECK_ERROR(vkCreateDescriptorSetLayout(device, &create_info, nullptr, &holder->layout));
    
    rhiDescriptorSetLayout rhi_layout;
    rhi_layout.native = holder->layout;
    rhi_layout.set_index = set_index;
    kept_descriptor_layouts.push_back(std::move(holder));

    return rhi_layout;
}

rhiDescriptorPool vkDeviceContext::create_descriptor_pool(const rhiDescriptorPoolCreateInfo& create_info, u32 max_sets)
{
    std::vector<VkDescriptorPoolSize> ps;
    ps.reserve(create_info.pool_sizes.size());
    for (auto& s : create_info.pool_sizes)
    {
        ps.push_back(
            {
                .type = vk_desc_type(s.type),
                .descriptorCount = s.count
            });
    }

    auto holder = std::make_shared<vkDescriptorPoolHolder>(device);
    VkDescriptorPoolCreateInfo ci
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = vk_desc_pool_create_flags(create_info.create_flags),
        .maxSets = max_sets,
        .poolSizeCount = static_cast<u32>(ps.size()),
        .pPoolSizes = ps.data(),
    };
    VK_CHECK_ERROR(vkCreateDescriptorPool(device, &ci, nullptr, &holder->pool));

    rhiDescriptorPool out{};
    out.native = holder->pool;
    kept_pools.push_back(std::move(holder));
    return out;
}

std::vector<rhiDescriptorSet> vkDeviceContext::allocate_descriptor_sets(rhiDescriptorPool pool, const std::vector<rhiDescriptorSetLayout>& layouts)
{
    auto vk_pool = reinterpret_cast<VkDescriptorPool>(pool.native);
    if (!vk_pool)
    {
        ASSERT(false);
        return {};
    }
    std::vector<VkDescriptorSetLayout> vk_layouts;
    vk_layouts.reserve(layouts.size());
    for (const rhiDescriptorSetLayout& layout : layouts)
    {
        auto vk_layout = reinterpret_cast<VkDescriptorSetLayout>(layout.native);
        if (!vk_layout)
        {
            ASSERT(false);
            continue;
        }
        vk_layouts.push_back(vk_layout);
    }

    std::vector<VkDescriptorSet> sets(layouts.size(), VK_NULL_HANDLE);
    const VkDescriptorSetAllocateInfo alloc_info
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vk_pool,
        .descriptorSetCount = static_cast<u32>(vk_layouts.size()),
        .pSetLayouts = vk_layouts.data()
    };
    VK_CHECK_ERROR(vkAllocateDescriptorSets(device, &alloc_info, sets.data()));

    std::vector<rhiDescriptorSet> result;
    result.reserve(sets.size());
    for (u32 index = 0; index < sets.size(); ++index)
    {
        result.push_back(
        {
            .native = sets[index],
            .set_index = layouts[index].set_index
        });
    }
    return result;
}

std::vector<rhiDescriptorSet> vkDeviceContext::allocate_descriptor_indexing_sets(rhiDescriptorPool pool, const std::vector<rhiDescriptorSetLayout>& layouts, const std::vector<u32>& counts)
{
    auto vk_pool = reinterpret_cast<VkDescriptorPool>(pool.native);
    if (!vk_pool)
    {
        ASSERT(false);
        return {};
    }
    std::vector<VkDescriptorSetLayout> vk_layouts;
    vk_layouts.reserve(layouts.size());
    for (const rhiDescriptorSetLayout& layout : layouts)
    {
        auto vk_layout = reinterpret_cast<VkDescriptorSetLayout>(layout.native);
        if (!vk_layout)
        {
            ASSERT(false);
            continue;
        }
        vk_layouts.push_back(vk_layout);
    }

    const VkDescriptorSetVariableDescriptorCountAllocateInfo count_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = static_cast<u32>(counts.size()),
        .pDescriptorCounts = counts.data()
    };

    std::vector<VkDescriptorSet> sets(layouts.size(), VK_NULL_HANDLE);
    const VkDescriptorSetAllocateInfo alloc_info
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &count_info,
        .descriptorPool = vk_pool,
        .descriptorSetCount = static_cast<u32>(vk_layouts.size()),
        .pSetLayouts = vk_layouts.data()
    };
    VK_CHECK_ERROR(vkAllocateDescriptorSets(device, &alloc_info, sets.data()));

    std::vector<rhiDescriptorSet> result;
    result.reserve(sets.size());
    for (u32 index = 0; index < sets.size(); ++index)
    {
        result.push_back(
            {
                .native = sets[index],
                .set_index = layouts[index].set_index
            });
    }
    return result;
}

void vkDeviceContext::update_descriptors(const std::vector<rhiWriteDescriptor>& writes)
{
    u32 total_img = 0, total_buf = 0;
    for (const auto& w : writes) 
    {
        const u32 n = std::max<u32>(1, w.count);
        switch (w.type) 
        {
        case rhiDescriptorType::sampler:
        case rhiDescriptorType::sampled_image:
        case rhiDescriptorType::combined_image_sampler:
        case rhiDescriptorType::storage_image:
            total_img += n;
            break;
        case rhiDescriptorType::uniform_buffer:
        case rhiDescriptorType::uniform_buffer_dynamic:
        case rhiDescriptorType::storage_buffer:
            total_buf += n;
            break;
        }
    }

    // temp buffer
    std::vector<VkDescriptorImageInfo> img_infos;
    img_infos.reserve(total_img);
    std::vector<VkDescriptorBufferInfo> buf_infos;
    buf_infos.reserve(total_buf);
    std::vector<VkWriteDescriptorSet> vk_writes;
    vk_writes.reserve(writes.size());

    for (const auto& write : writes)
    {
        VkWriteDescriptorSet write_set
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = reinterpret_cast<VkDescriptorSet>(write.set.native),
            .dstBinding = write.binding,
            .dstArrayElement = write.array_index,
            .descriptorCount = write.count,
            .descriptorType = vk_desc_type(write.type),
        };
        switch (write.type)
        {
        case rhiDescriptorType::sampler:
        case rhiDescriptorType::sampled_image:
        case rhiDescriptorType::combined_image_sampler:
        case rhiDescriptorType::storage_image:
        {
            const u32 size = static_cast<u32>(img_infos.size());
            const u32 n = std::max<u32>(1, write.count);
            for (u32 index = 0; index < n; ++index)
            {
                const rhiDescriptorImageInfo& img = write.image[index];
                VkDescriptorImageInfo img_info{};
                if (write.type == rhiDescriptorType::sampler)
                {
                    img_info.sampler = get_vk_sampler(img.sampler);
                    img_info.imageView = VK_NULL_HANDLE;
                    img_info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                }
                else if (write.type == rhiDescriptorType::sampled_image
                    || write.type == rhiDescriptorType::storage_image)
                {
                    img_info.sampler = VK_NULL_HANDLE;
                    if (img.texture)
                    {
                        auto vk_tex = static_cast<vkTexture*>(img.texture);
                        if (img.is_separate_depth_view)
                            img_info.imageView = vk_tex->get_depth_view();
                        else
                            img_info.imageView = vk_tex->get_view();
                    }
                    else if (img.texture_cubemap)
                    {
                        auto vk_tex_cube = static_cast<vkTextureCubemap*>(img.texture_cubemap);
                        if (img.cubemap_viewtype == rhiCubemapViewType::mip)
                            img_info.imageView = vk_tex_cube->get_mip_view(img.mip);
                        else
                            img_info.imageView = vk_tex_cube->get_cube_view();
                    }
                    
                    img_info.imageLayout = vk_layout(img.layout);
                }
                else if (write.type == rhiDescriptorType::combined_image_sampler)
                {
                    auto vk_tex = static_cast<vkTexture*>(img.texture);
                    img_info.sampler = get_vk_sampler(img.sampler);
                    img_info.imageView = vk_tex->get_view();
                    img_info.imageLayout = vk_layout(img.layout);
                }
                img_infos.emplace_back(img_info);
            }
            write_set.pImageInfo = img_infos.data() + size;
            break;
        }
        case rhiDescriptorType::uniform_buffer:
        case rhiDescriptorType::uniform_buffer_dynamic:
        case rhiDescriptorType::storage_buffer:
        {
            const u32 size = static_cast<u32>(buf_infos.size());
            const u32 n = std::max<u32>(1, write.count);
            for (u32 index = 0; index < n; ++index)
            {
                const rhiDescriptorBufferInfo& buf = write.buffer[index];
                const VkDescriptorBufferInfo buf_info
                {
                    .buffer = reinterpret_cast<VkBuffer>(buf.buffer->native()),
                    .offset = buf.offset,
                    .range = buf.range
                };
                buf_infos.emplace_back(buf_info);
            }
            write_set.pBufferInfo = buf_infos.data() + size;
            break;
        }
        }
        vk_writes.push_back(write_set);
    }
    vkUpdateDescriptorSets(device, static_cast<u32>(vk_writes.size()), vk_writes.data(), 0, nullptr);
}

rhiPipelineLayout vkDeviceContext::create_pipeline_layout(std::vector<rhiDescriptorSetLayout> set_layouts, std::vector<rhiPushConstant> push_constant_bytes, void** keep_alive_out)
{
    std::shared_ptr<vkPipelineLayoutHolder> keep;
    auto layout = vk_create_pipeline_layout(device, set_layouts, push_constant_bytes, keep);
    if (keep_alive_out)
    {
        *keep_alive_out = keep.get();
    }
    kept_pipeline_layouts.push_back(std::move(keep));
    return layout;
}

std::unique_ptr<rhiPipeline> vkDeviceContext::create_graphics_pipeline(const rhiGraphicsPipelineDesc& desc, const rhiPipelineLayout& layout)
{
    return vk_create_graphics_pipeline(device, desc, layout);
}

std::unique_ptr<rhiPipeline> vkDeviceContext::create_compute_pipeline(const rhiComputePipelineDesc& desc, const rhiPipelineLayout& layout)
{
    return vk_create_compute_pipeline(device, desc, layout);
}

std::shared_ptr<rhiCommandList> vkDeviceContext::begin_onetime_commands(rhiQueueType t)
{
    ASSERT(queue.contains(t));
    
    const VkCommandPoolCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queue[t]->q_index()
    };
    VkCommandPool cmd_pool;
    VK_CHECK_ERROR(vkCreateCommandPool(device, &create_info, nullptr, &cmd_pool));

    const VkCommandBufferAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmd;
    VK_CHECK_ERROR(vkAllocateCommandBuffers(device, &alloc_info, &cmd));

    const VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK_ERROR(vkBeginCommandBuffer(cmd, &begin_info));

    return std::make_shared<vkCommandList>(this, cmd_pool, cmd, true);
}

void vkDeviceContext::submit_and_wait(std::shared_ptr<rhiCommandList> cmd, rhiQueueType t)
{
    ASSERT(queue.contains(t));

    auto vk_cmdlst = std::dynamic_pointer_cast<vkCommandList>(cmd);
    ASSERT(vk_cmdlst);
    auto vk_cmd = vk_cmdlst->get_cmd_buffer();
    ASSERT(vk_cmd != VK_NULL_HANDLE);
    VK_CHECK_ERROR(vkEndCommandBuffer(vk_cmd));

    const VkFenceCreateInfo fence_ci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence submit_fence;
    VK_CHECK_ERROR(vkCreateFence(device, &fence_ci, nullptr, &submit_fence));

    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &vk_cmd
    };
    auto q = reinterpret_cast<VkQueue>(queue[t]->handle());
    ASSERT(q != VK_NULL_HANDLE);
    VK_CHECK_ERROR(vkQueueSubmit(q, 1, &submit_info, submit_fence));
    VK_CHECK_ERROR(vkWaitForFences(device, 1, &submit_fence, VK_TRUE, UINT64_MAX));
    vkDestroyFence(device, submit_fence, nullptr);

    if (vk_cmdlst->is_transient_())
    {
        vkFreeCommandBuffers(device, vk_cmdlst->get_cmd_pool(), 1, &vk_cmd);
        vk_cmdlst.reset();
    }
}

void vkDeviceContext::submit(rhiQueueType type, const rhiSubmitInfo& info)
{
    ASSERT(queue.contains(type));

    std::vector<VkSemaphoreSubmitInfo> wait_semaphores;
    wait_semaphores.reserve(info.waits.size());
    for (auto& wait : info.waits)
    {
        auto* semaphore = static_cast<vkSemaphore*>(wait.semaphore);
        ASSERT(semaphore);
        wait_semaphores.push_back(VkSemaphoreSubmitInfo{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = semaphore->handle(),
                .value = wait.value,
                .stageMask = vk_pipeline_stage2(wait.stage),
                .deviceIndex = 0
            });
    }

    std::vector<VkSemaphoreSubmitInfo> signal_semaphores;
    signal_semaphores.reserve(info.signals.size());
    for (auto& signal : info.signals)
    {
        auto* semaphore = static_cast<vkSemaphore*>(signal.semaphore);
        ASSERT(semaphore);
        signal_semaphores.push_back(VkSemaphoreSubmitInfo{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = semaphore->handle(),
                .value = signal.value,
                .stageMask = vk_pipeline_stage2(signal.stage),
                .deviceIndex = 0
            });
    }

    std::vector<VkCommandBufferSubmitInfo> cmds;
    cmds.reserve(info.cmd_lists.size());
    for (auto& cmd : info.cmd_lists)
    {
        auto* cmd_list = static_cast<vkCommandList*>(cmd);
        ASSERT(cmd_list);
        cmds.push_back(VkCommandBufferSubmitInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = cmd_list->get_cmd_buffer(),
                .deviceMask = 0
            });
    }

    const VkSubmitInfo2 submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = static_cast<u32>(wait_semaphores.size()),
        .pWaitSemaphoreInfos = wait_semaphores.data(),
        .commandBufferInfoCount = static_cast<u32>(cmds.size()),
        .pCommandBufferInfos = cmds.data(),
        .signalSemaphoreInfoCount = static_cast<u32>(signal_semaphores.size()),
        .pSignalSemaphoreInfos = signal_semaphores.data()
    };

    VkFence vk_fence = VK_NULL_HANDLE;
    if (info.fence)
        vk_fence = static_cast<vkFence*>(info.fence)->handle();

    VkQueue q = reinterpret_cast<VkQueue>(queue[type]->handle());
    VK_CHECK_ERROR(vkQueueSubmit2(q, 1, &submit_info, vk_fence));
}

void vkDeviceContext::wait(rhiFence* f)
{
    auto vk_f = static_cast<vkFence*>(f);
    ASSERT(vk_f);
    VkFence vk_fence = vk_f->handle();
    vkWaitForFences(device, 1, &vk_fence, VK_TRUE, UINT64_MAX);
}

void vkDeviceContext::reset(rhiFence* f)
{
    auto vk_f = static_cast<vkFence*>(f);
    ASSERT(vk_f);
    VkFence vk_fence = vk_f->handle();
    vkResetFences(device, 1, &vk_fence);
}

void vkDeviceContext::create_imageview_cache()
{
    imageview_cache = std::make_shared<vkImageViewCache>(device);
}

void vkDeviceContext::create_vma_allocator(VkInstance instance)
{
    VmaVulkanFunctions vma_funcs{};
    vma_funcs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vma_funcs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    const VmaAllocatorCreateInfo vma_alloc_crate_info{
        .flags = 0,
        .physicalDevice = phys_device,
        .device = device,
        .pVulkanFunctions = &vma_funcs,
        .instance = instance,
    };
    VK_CHECK_ERROR(vmaCreateAllocator(&vma_alloc_crate_info, &allocator));
}

void vkDeviceContext::create_queue(const std::unordered_map<rhiQueueType, u32>& queue_families)
{
    for (const auto& [key, value] : queue_families)
    {
        auto q = std::make_shared<vkQueue>(device, key, value);
        queue.emplace(key, std::move(q));
    }
}

rhiDescriptorSetLayout vkDeviceContext::create_descriptor_indexing_set_layout(const rhiDescriptorIndexing& desc, const u32 set_index)
{
    std::vector<VkDescriptorSetLayoutBinding> binding_layouts;
    std::vector<VkDescriptorBindingFlags> binding_flags;
    binding_layouts.reserve(desc.elements.size());
    binding_flags.reserve(desc.elements.size());
    for (const auto& d : desc.elements)
    {
        const VkDescriptorSetLayoutBinding b{
            .binding = d.binding,
            .descriptorType = vk_desc_type(d.type),
            .descriptorCount = d.descriptor_count,
            .stageFlags = vk_shader_stage(d.stage)
        };
        binding_layouts.push_back(b);

        const VkDescriptorBindingFlags flags = vk_binding_flags(d.binding_flags);
        binding_flags.push_back(flags);
    }

    const VkDescriptorSetLayoutBindingFlagsCreateInfo flags_createinfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = static_cast<u32>(binding_flags.size()),
        .pBindingFlags = binding_flags.data()
    };
    const VkDescriptorSetLayoutCreateInfo layout_createinfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &flags_createinfo,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = static_cast<u32>(binding_layouts.size()),
        .pBindings = binding_layouts.data(),
    };
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VK_CHECK_ERROR(vkCreateDescriptorSetLayout(device, &layout_createinfo, nullptr, &set_layout));

    auto holder = std::make_shared<vkDescriptorSetLayoutHolder>(device);
    holder->layout = set_layout;
    holder->bindings = std::move(binding_layouts);

    rhiDescriptorSetLayout rhi_layout;
    rhi_layout.native = holder->layout;
    rhi_layout.set_index = set_index;
    kept_descriptor_layouts.push_back(std::move(holder));

    return rhi_layout;
}
