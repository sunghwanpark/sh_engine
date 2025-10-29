#pragma once

#include "drawPass.h"
#include "renderShared.h"

class rhiBuffer;
struct meshletBuffer;

struct meshletDrawUpdateContext : public drawUpdateContext
{
	rhiBuffer* global_buf;
	meshletBuffer* meshlet_buf;
};

class meshletDrawPass : public drawPass
{
public:
	void draw(rhiCommandList* cmd) override;
	void update(drawUpdateContext* update_context) override;

public:
	void update_elements(groupRecordArray* group_records, drawTypeBuffers* instance_buf, drawTypeBuffers* draw_param_buf, drawTypeBuffers* indirect_buf);

protected:
	void build_meshlet_descriptor_layout(const u32 layout_index);

protected:
	groupRecordArray* group_records;
	drawTypeBuffers* indirect_buffer;
	drawTypeBuffers* draw_params_buffer;
	drawTypeBuffers* instance_buffer;

	u32 meshlet_layout_index = 0;
	rhiDescriptorSetLayout layout_meshlet;
	drawType draw_type = drawType::gbuffer;
};