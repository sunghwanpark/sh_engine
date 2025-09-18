#pragma once

#include "drawPass.h"

struct skyInitContext : public drawInitContext
{
    std::weak_ptr<rhiTexture> scene_color;
    virtual std::unique_ptr<drawInitContext> clone() const
    {
        return std::make_unique<skyInitContext>(*this);
    }
};

struct skyUpdateContext : public drawUpdateContext
{
public:
    skyUpdateContext(rhiBuffer* global_buf) : global_buf(global_buf) {}

    rhiBuffer* global_buf;
};

class skyPass final : public drawPass
{
public:
    struct alignas(16) convertCB
    {
        u32 dst_size;
        u32 mip_level;
    };

    struct alignas(16) irradianceCB
    {
        u32 out_size;        // out cube face resolution (32) 
        u32 num_samples;     // irradiance samples (256~1024)
    };

    struct alignas(16) specularCB
    {
        u32 mip_level;
        u32 num_samples;
        f32 roughness;
        f32 _pad;
    }; // 16b

    struct alignas(16) brdfCB
    {
        u32 num_samples;
    };

public:
    void initialize(const drawInitContext& context) override;
    void update(drawUpdateContext* update_context) override;

public:
    rhiTextureCubeMap* get_irradiance_map() { return irradiance_cubemap.get(); }
    rhiTextureCubeMap* get_specular_map() { return specular_cubemap.get(); }
    rhiTexture* get_brdf_lut_map() { return brdf_lut.get(); }
    const u32 get_cubemap_mip_count() const;

protected:
    void draw(rhiCommandList* cmd) override;
    void begin_barrier(rhiCommandList* cmd) override;
    void end_barrier(rhiCommandList* cmd) override;
    void build_layouts(renderShared* rs) override;
    void build_attachments(rhiDeviceContext* context) override;
    void build_pipeline(renderShared* rs) override;

private:
    void precompile_dispatch();

private:
    std::shared_ptr<rhiTextureCubeMap> sky_cubemap;
    std::shared_ptr<rhiTextureCubeMap> irradiance_cubemap;
    std::shared_ptr<rhiTextureCubeMap> specular_cubemap;
    std::shared_ptr<rhiTexture> brdf_lut;
    std::unique_ptr<rhiBuffer> irradiance_cb;

    rhiDescriptorSetLayout set_fragment_layout;
    bool is_first_frame = true;
};
