#pragma once

#include "pch.h"
#include "rhi/rhiDefs.h"
#include "rhi/rhiTextureView.h"

struct rhiClearValue 
{
    f32 rgba[4] = { 0,0,0,1 };
    f32 depth = 1.0f;
    u32 stencil = 0;
};

struct renderTargetViewDesc
{
    rhiTexture* tex;
    rhiFormat format;
    rhiImageAspect aspect;
    u32 mip;
    u32 base_layer;
    u32 layer_count;
};

struct rhiRenderingAttachment 
{
    std::optional<rhiRenderTargetView> view;
    rhiLoadOp load_op = rhiLoadOp::load;
    rhiStoreOp store_op = rhiStoreOp::store;
    rhiClearValue clear{};
};

struct rhiRenderingInfo 
{
    std::string renderpass_name = "";
    vec4 render_area;
    std::vector<rhiRenderingAttachment> color_attachments;

    std::optional<rhiRenderingAttachment> depth_attachment;
    std::optional<rhiRenderingAttachment> stencil_attachment; 

    std::vector<rhiFormat> color_formats;
    std::optional<rhiFormat> depth_format;
    rhiSampleCount samples = rhiSampleCount::x1;
    u32 layer_count = 1;
};