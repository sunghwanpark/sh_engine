#include "indirectDrawPass.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiBuffer.h"
#include "rhi/rhiCommandList.h"

void indirectDrawPass::update_elements(std::vector<groupRecord>* indirect_args, std::weak_ptr<rhiBuffer> instance_buf, std::weak_ptr<rhiBuffer> indirect_buf)
{
	group_records = indirect_args;
	instance_buffer = instance_buf;
	indirect_buffer = indirect_buf;
}

void indirectDrawPass::update_instances(renderShared* rs, const u32 instancebuf_desc_idx)
{
    if (auto buf = instance_buffer.lock())
    {
        const rhiDescriptorBufferInfo buffer_info{
            .buffer =buf.get(),
            .offset = 0,
            .range = buf->size()
        };
        const rhiWriteDescriptor write_desc{
            .set = descriptor_sets[image_index.value()][instancebuf_desc_idx],
            .binding = 0,
            .array_index = 0,
            .count = 1,
            .type = rhiDescriptorType::storage_buffer,
            .buffer = { buffer_info }
        };
        rs->context->update_descriptors({ write_desc });
    }
}

void indirectDrawPass::draw(rhiCommandList* cmd)
{
    auto buffer_ptr = indirect_buffer.lock();
    cmd->bind_pipeline(pipeline.get());

    constexpr u32 stride = sizeof(rhiDrawIndexedIndirect);
    for (const auto& g : *group_records)
    {
        cmd->bind_vertex_buffer(const_cast<rhiBuffer*>(g.vbo), 0, 0);
        cmd->bind_index_buffer(const_cast<rhiBuffer*>(g.ibo), 0);
        const u32 byte_offset = g.first_cmd * stride;
        cmd->draw_indexed_indirect(buffer_ptr.get(), byte_offset, g.cmd_count, stride);
    }
}