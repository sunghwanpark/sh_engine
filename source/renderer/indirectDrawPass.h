#pragma once

#include "drawPass.h"
#include "renderShared.h"

class rhiBuffer;
class indirectDrawPass : public drawPass
{
public:
	void draw(rhiCommandList* cmd) override;

public:
	void update_elements(groupRecordArray* group_records, drawTypeBuffers* instance_buf, drawTypeBuffers* indirect_buf);
	void update_double_sided_info(std::unordered_map<u32, bool> infos) { double_sided_infos = infos; }
	virtual void update_instances(renderShared* rs, const u32 instancebuf_desc_idx);

protected:
	groupRecordArray* group_records;
	std::unordered_map<u32, bool> double_sided_infos;
	drawTypeBuffers* indirect_buffer;
	drawTypeBuffers* instance_buffer;

	drawType draw_type = drawType::gbuffer;
};