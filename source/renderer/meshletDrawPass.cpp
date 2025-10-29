#include "meshletDrawPass.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiBuffer.h"
#include "rhi/rhiCommandList.h"
#include "meshlet/meshletDef.h"

void meshletDrawPass::update_elements(groupRecordArray* group_rec, drawTypeBuffers* instance_buf, drawTypeBuffers* draw_param_buf, drawTypeBuffers* indirect_buf)
{
	group_records = group_rec;
	instance_buffer = instance_buf;
    draw_params_buffer = draw_param_buf;
	indirect_buffer = indirect_buf;
}

void meshletDrawPass::build_meshlet_descriptor_layout(const u32 layout_index)
{
    meshlet_layout_index = layout_index;
    layout_meshlet = init_context->rs->context->create_descriptor_set_layout(
        {
            rhiDescriptorSetLayoutBinding{
                .binding = 0,
                .type = rhiDescriptorType::storage_buffer,
                .count = 1,
                .stage = rhiShaderStage::mesh
            },
            rhiDescriptorSetLayoutBinding{
                .binding = 1,
                .type = rhiDescriptorType::storage_buffer,
                .count = 1,
                .stage = rhiShaderStage::mesh
            },
            rhiDescriptorSetLayoutBinding{
                .binding = 2,
                .type = rhiDescriptorType::storage_buffer,
                .count = 1,
                .stage = rhiShaderStage::mesh
            },
            rhiDescriptorSetLayoutBinding{
                .binding = 3,
                .type = rhiDescriptorType::storage_buffer,
                .count = 1,
                .stage = rhiShaderStage::mesh
            },
            rhiDescriptorSetLayoutBinding{
                .binding = 4,
                .type = rhiDescriptorType::storage_buffer,
                .count = 1,
                .stage = rhiShaderStage::mesh
            },
            rhiDescriptorSetLayoutBinding{
                .binding = 5,
                .type = rhiDescriptorType::storage_buffer,
                .count = 1,
                .stage = rhiShaderStage::mesh
            },
            rhiDescriptorSetLayoutBinding{
                .binding = 6,
                .type = rhiDescriptorType::storage_buffer,
                .count = 1,
                .stage = rhiShaderStage::mesh
            },
            rhiDescriptorSetLayoutBinding{
                .binding = 7,
                .type = rhiDescriptorType::storage_buffer,
                .count = 1,
                .stage = rhiShaderStage::mesh
            },
            rhiDescriptorSetLayoutBinding{
                .binding = 8,
                .type = rhiDescriptorType::storage_buffer,
                .count = 1,
                .stage = rhiShaderStage::mesh
            },
        }, layout_index);
}

void meshletDrawPass::draw(rhiCommandList* cmd)
{
    auto& buf = indirect_buffer->at(static_cast<u8>(draw_type));
    cmd->bind_pipeline(pipeline.get());

    constexpr u32 stride = sizeof(rhiDrawMeshShaderIndirect);
    const auto group = group_records->at(static_cast<u8>(draw_type));
    for (const auto& g : group)
    {
        const u32 byte_offset = g.first_cmd * stride;
        cmd->draw_mesh_tasks_indirect(buf.get(), byte_offset, g.cmd_count, stride);
    }
}

void meshletDrawPass::update(drawUpdateContext* update_context)
{
    auto ctx = static_cast<meshletDrawUpdateContext*>(update_context);
    ASSERT(ctx);

    std::vector<rhiWriteDescriptor> write_descriptors;
    write_descriptors.push_back(rhiWriteDescriptor{
        .set = descriptor_sets[image_index.value()][0],
        .binding = 0,
        .array_index = 0,
        .count = 1,
        .type = rhiDescriptorType::uniform_buffer,
        .buffer = { 
            rhiDescriptorBufferInfo{
                .buffer = ctx->global_buf,
                .offset = 0,
                .range = sizeof(globalsCB)
            }
        }
    });

    //if (is_first_frame)
    {
        auto create_write_desc = [](rhiDescriptorSet& sets, const u32 binding_slot, rhiBuffer* buf, const u32 range) -> rhiWriteDescriptor
            {
                return rhiWriteDescriptor{
                    .set = sets,
                    .binding = binding_slot,
                    .count = 1,
                    .type = rhiDescriptorType::storage_buffer,
                    .buffer = {
                        rhiDescriptorBufferInfo{
                            .buffer = buf,
                            .offset = 0,
                            .range = range
                        }}
                };
            };

        auto& instance_buf = instance_buffer->at(static_cast<u8>(draw_type));
        if(instance_buf)
            write_descriptors.push_back(create_write_desc(descriptor_sets[image_index.value()][meshlet_layout_index], 0, instance_buf.get(), static_cast<u32>(instance_buf->size())));
        auto& drawparam_buf = draw_params_buffer->at(static_cast<u8>(draw_type));
        if (drawparam_buf)
            write_descriptors.push_back(create_write_desc(descriptor_sets[image_index.value()][meshlet_layout_index], 1, drawparam_buf.get(), static_cast<u32>(drawparam_buf->size())));
        if (ctx->meshlet_buf)
        {
            write_descriptors.push_back(create_write_desc(descriptor_sets[image_index.value()][meshlet_layout_index],
                2, ctx->meshlet_buf->pos.get(), static_cast<u32>(ctx->meshlet_buf->pos->size())));
            write_descriptors.push_back(create_write_desc(descriptor_sets[image_index.value()][meshlet_layout_index],
                3, ctx->meshlet_buf->norm.get(), static_cast<u32>(ctx->meshlet_buf->norm->size())));
            write_descriptors.push_back(create_write_desc(descriptor_sets[image_index.value()][meshlet_layout_index],
                4, ctx->meshlet_buf->uv.get(), static_cast<u32>(ctx->meshlet_buf->uv->size())));
            write_descriptors.push_back(create_write_desc(descriptor_sets[image_index.value()][meshlet_layout_index],
                5, ctx->meshlet_buf->tan.get(), static_cast<u32>(ctx->meshlet_buf->tan->size())));

            write_descriptors.push_back(create_write_desc(descriptor_sets[image_index.value()][meshlet_layout_index],
                6, ctx->meshlet_buf->header.get(), static_cast<u32>(ctx->meshlet_buf->header->size())));
            write_descriptors.push_back(create_write_desc(descriptor_sets[image_index.value()][meshlet_layout_index],
                7, ctx->meshlet_buf->vert_indices.get(), static_cast<u32>(ctx->meshlet_buf->vert_indices->size())));
            write_descriptors.push_back(create_write_desc(descriptor_sets[image_index.value()][meshlet_layout_index],
                8, ctx->meshlet_buf->tri_bytes.get(), static_cast<u32>(ctx->meshlet_buf->tri_bytes->size())));
        }
    }

    init_context->rs->context->update_descriptors(write_descriptors);
}