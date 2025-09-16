#pragma once

#include "pch.h"
#include "rhi/rhiDefs.h"
#include "rhi/rhiShader.h"

struct rhiPipelineLayout 
{ 
	void* native = nullptr; 
};

struct rhiVertexAttributeDesc
{
    u32 location;
    u32 binding;
    rhiFormat format;
    u32 offset;
};

struct rhiVertexAttribute
{
    u32 binding;
    u32 stride;
    std::vector<rhiVertexAttributeDesc> vertex_attr_desc;
};

struct rhiGraphicsPipelineDesc 
{
    std::optional<rhiShaderBinary> vs;
    std::optional<rhiShaderBinary> fs;
    std::vector<rhiFormat> color_formats;
    std::optional<rhiFormat> depth_format;
    rhiSampleCount samples = rhiSampleCount::x1;

    bool depth_test = true;
    bool depth_write = true;
    bool use_dynamic_cullmode = false;

    rhiVertexAttribute vertex_layout;
    // todo
    // blend / raster
};

struct rhiComputePipelineDesc 
{
    rhiShaderBinary cs;
};

class rhiPipeline 
{
public:
    virtual ~rhiPipeline() = default;
    rhiPipelineType pipeline_type() const { return type; }

protected:
    explicit rhiPipeline(const rhiGraphicsPipelineDesc& desc, rhiPipelineType t) : desc(desc), type(t) {}
    explicit rhiPipeline(const rhiComputePipelineDesc& desc, rhiPipelineType t) : type(t) {}

    rhiGraphicsPipelineDesc desc;
    rhiPipelineType type;
};