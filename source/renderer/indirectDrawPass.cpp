#include "indirectDrawPass.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiBuffer.h"
#include "rhi/rhiCommandList.h"

void indirectDrawPass::update_elements(groupRecordArray* group_rec, drawTypeBuffers* instance_buf, drawTypeBuffers* indirect_buf)
{
	group_records = group_rec;
	instance_buffer = instance_buf;
	indirect_buffer = indirect_buf;
}

void indirectDrawPass::update_instances(renderShared* rs, const u32 instancebuf_desc_idx)
{
    auto& instance_buf = instance_buffer->at(static_cast<u8>(draw_type));
    if (instance_buf == nullptr)
        return;

    const rhiDescriptorBufferInfo instance_buffer_info{
        .buffer = instance_buf.get(),
        .offset = 0,
        .range = instance_buf->size()
    };
    const rhiWriteDescriptor instance_write_desc{
        .set = descriptor_sets[image_index.value()][instancebuf_desc_idx],
        .binding = 0,
        .array_index = 0,
        .count = 1,
        .type = rhiDescriptorType::storage_buffer,
        .buffer = { instance_buffer_info }
    };

    rs->context->update_descriptors({ instance_write_desc });
}

void indirectDrawPass::draw(rhiCommandList* cmd)
{
    auto& buf = indirect_buffer->at(static_cast<u8>(draw_type));
    cmd->bind_pipeline(pipeline.get());

    constexpr u32 stride = sizeof(rhiDrawIndexedIndirect);
    const auto group = group_records->at(static_cast<u8>(draw_type));
    for (const auto& g : group)
    {
        cmd->bind_vertex_buffer(const_cast<rhiBuffer*>(g.vbo), 0, 0);
        cmd->bind_index_buffer(const_cast<rhiBuffer*>(g.ibo), 0);
        const u32 byte_offset = g.first_cmd * stride;
        cmd->draw_indexed_indirect(buf.get(), byte_offset, g.cmd_count, stride);
    }
}