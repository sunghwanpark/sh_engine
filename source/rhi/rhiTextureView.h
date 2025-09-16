#pragma once

#include "pch.h"
#include "rhi/rhiDefs.h"

struct rhiImageSubresourceLayers 
{
    rhiImageAspect aspect;
    u32 mip_level = 0;
    u32 base_array_layer = 0;
    u32 layer_count = 1;
};

struct rhiBufferImageCopy
{
    u64 buffer_offset = 0;
    u32 buffer_rowlength = 0;
    u32 buffer_imageheight = 0; 

    rhiImageSubresourceLayers imageSubresource;
    i32vec3 image_offset{ 0, 0, 0 };
    u32vec3 image_extent{ 0, 0, 1 };
};

struct rhiGenMipsDesc 
{
    u32 base_mip = 0;
    u32 mip_count = 0; 
    u32 base_layer = 0;
    u32 layer_count = 0; 
    rhiMipsMethod method = rhiMipsMethod::auto_select;
};

struct rhiTextureDesc 
{
    u32 width = 1;
    u32 height = 1;
    u32 base_layer = 0;
    u32 layers = 1;
    u32 base_mip = 0;
    u32 mips = 1;
    rhiFormat format = rhiFormat::RGBA8_UNORM;
    rhiSampleCount samples = rhiSampleCount::x1;
    bool is_depth = false;
    bool is_separate_depth_stencil = false;
};

class rhiTexture 
{
public:
    rhiTexture(const rhiTextureDesc& desc) : desc(desc) {}
    rhiTexture(class rhiDeviceContext* context, std::string_view path, bool is_hdr = false);
    virtual ~rhiTexture() = default;

protected:
    void generate_mips(rhiDeviceContext* context);

private:
    void generate_equirect(std::string_view path);

public:
    rhiTextureDesc desc;

protected:
    stbi_uc* pixels = nullptr;
    std::vector<f32> rgba;
};

class rhiTextureCubeMap
{
public:
    rhiTextureCubeMap(const rhiTextureDesc& desc) : desc(desc) {}
    virtual ~rhiTextureCubeMap() = default;

public:
    rhiTextureDesc desc;
};

struct rhiRenderTargetView 
{
    rhiTexture* texture;
    u32 mip = 0;
    u32 base_layer = 0;
    u32 layer_count = 1;
    rhiImageLayout layout = rhiImageLayout::color_attachment;
};