#pragma once

#include "pch.h"
#include "rhiDefs.h"

struct rhiBufferBarrierDescription
{
	rhiPipelineStage src_stage;
	rhiPipelineStage dst_stage;
	rhiAccessFlags src_access;
	rhiAccessFlags dst_access;
	u32 offset = 0;
	u64 size;
	u32 src_queue = 0;
	u32 dst_queue = 0;
};

struct rhiImageBarrierDescription
{
	rhiPipelineStage src_stage;
	rhiPipelineStage dst_stage;
	rhiAccessFlags src_access;
	rhiAccessFlags dst_access;
	rhiImageLayout old_layout;
	rhiImageLayout new_layout;
	u32 src_queue = 0;
	u32 dst_queue = 0;
	u32 base_mip = 0;
	u32 level_count = 1;
	u32 base_layer = 0;
	u32 layer_count = 1;
};

class rhiSemaphore 
{ 
public:
	virtual ~rhiSemaphore() = default;
};

class rhiFence
{ 
public: 
	virtual ~rhiFence() = default; 
};