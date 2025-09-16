#include "rhiTextureView.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiCommandList.h"

rhiTexture::rhiTexture(rhiDeviceContext* context, std::string_view path, bool is_hdr)
{
    if (is_hdr)
    {
        generate_equirect(path);
        return;
    }

    i32 width = 0;
    i32 height = 0;
    i32 channels = 0;
    pixels = stbi_load(path.data(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
        throw std::runtime_error("failed to load png");

    auto calc_mip_count = [](const u32 width, const u32 height) -> u32
        {
            u32 mip_count = 1;
            while ((width | height) >> mip_count)
                ++mip_count;
            return mip_count;
        };

    desc.width = static_cast<u32>(width);
    desc.height = static_cast<u32>(height);
    desc.layers = 1;
    desc.mips = calc_mip_count(width, height);
    desc.format = rhiFormat::RGBA8_SRGB;
    desc.samples = rhiSampleCount::x1;
    desc.is_depth = false;
}

void rhiTexture::generate_mips(rhiDeviceContext* context)
{
    u32 image_size = desc.width * desc.height * 4;

    auto staging = context->create_buffer(rhiBufferDesc{
        .size = image_size,
        .usage = rhiBufferUsage::transfer_src,
        .memory = rhiMem::auto_host,
        });
    void* mapped = staging->map();
    std::memcpy(mapped, pixels, image_size);

    auto cmd_lst = context->begin_onetime_commands();
    cmd_lst->image_barrier(this, rhiImageLayout::undefined, rhiImageLayout::transfer_dst, 0, desc.mips, 0, desc.layers);

    // buffer → image (mip 0)
    const rhiBufferImageCopy c{
        .buffer_offset = 0,
        .buffer_rowlength = 0,
        .buffer_imageheight = 0,
        .imageSubresource = {
            .aspect = rhiImageAspect::color,
            .mip_level = 0,
            .base_array_layer = 0,
            .layer_count = 1
        },
        .image_offset = {0,0,0},
        .image_extent = {desc.width, desc.height, 1}
    };
    cmd_lst->copy_buffer_to_image(staging.get(), this, rhiImageLayout::transfer_dst, { &c, 1 });

    if (desc.mips > 1)
    {
        cmd_lst->image_barrier(this, rhiImageLayout::transfer_dst, rhiImageLayout::transfer_src, 0, 1, 0, desc.layers);
        cmd_lst->generate_mips(this, {
            .base_mip = 0,
            .mip_count = desc.mips,
            .base_layer = 0,
            .layer_count = desc.layers,
            .method = rhiMipsMethod::auto_select
            });
    }
    else
    {
        cmd_lst->image_barrier(this, rhiImageLayout::transfer_src, rhiImageLayout::shader_readonly, 0, 1, 0, desc.layers);
    }
    context->submit_and_wait(cmd_lst);
    stbi_image_free(pixels);
}

void rhiTexture::generate_equirect(std::string_view path)
{
    i32 width = 0;
    i32 height = 0;
    i32 channels = 0;
    stbi_set_flip_vertically_on_load(false);
    auto f_pixels = stbi_loadf(path.data(), &width, &height, &channels, 3);
    if (!f_pixels)
        throw std::runtime_error("failed to load hdr");

    rgba.resize(width * height * 4);
    for (i32 i = 0; i < width * height; ++i)
    {
        rgba[i * 4 + 0] = f_pixels[i * 3 + 0];
        rgba[i * 4 + 1] = f_pixels[i * 3 + 1];
        rgba[i * 4 + 2] = f_pixels[i * 3 + 2];
        rgba[i * 4 + 3] = 1.f;
    }

    desc.width = static_cast<u32>(width);
    desc.height = static_cast<u32>(height);
    desc.layers = 1;
    desc.mips = 1;
    desc.format = rhiFormat::RGBA32_SFLOAT;
    desc.samples = rhiSampleCount::x1;
    desc.is_depth = false;
    stbi_image_free(f_pixels);
}
