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

public:
    void initialize(const drawInitContext& context) override;
    void update(drawUpdateContext* update_context) override;

protected:
    void draw(rhiCommandList* cmd) override;
    void begin_barrier(rhiCommandList* cmd) override;
    void end_barrier(rhiCommandList* cmd) override;
    void build_layouts(renderShared* rs) override;
    void build_attachments(rhiDeviceContext* context) override;
    void build_pipeline(renderShared* rs) override;

private:
    void fill_skycubemap();

private:
    std::shared_ptr<rhiTextureCubeMap> sky_cubemap;
    rhiDescriptorSetLayout set_fragment_layout;
    bool is_first_frame = true;
};
