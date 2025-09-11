#pragma once

#include "drawPass.h"
#include "renderShared.h"

class rhiBuffer;
class indirectDrawPass : public drawPass
{
public:
	void draw(rhiCommandList* cmd) override;

public:
	void update_elements(std::vector<groupRecord>* indirect_args, std::weak_ptr<rhiBuffer> instance_buf, std::weak_ptr<rhiBuffer> indirect_buf);
	void update_instances(renderShared* rs, const u32 instancebuf_desc_idx);

protected:
	std::vector<groupRecord>* group_records;
	std::weak_ptr<rhiBuffer> indirect_buffer;
	std::weak_ptr<rhiBuffer> instance_buffer;
};