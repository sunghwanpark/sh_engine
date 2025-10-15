#pragma once

#include "drawPass.h"

struct oitInitContext : public drawInitContext
{
    std::weak_ptr<rhiTexture> scene_color;
    virtual std::unique_ptr<drawInitContext> clone() const
    {
        return std::make_unique<oitInitContext>(*this);
    }
};

struct oitUpdateContext : public drawUpdateContext
{
    rhiTexture* accum;
    rhiTexture* reveal;
};

class oitResolvePass final : public drawPass 
{
public:
    void update(drawUpdateContext* update_context) override;
    void draw(rhiCommandList* cmd) override;

protected:
    void build_layouts(renderShared* rs) override;
    void build_attachments(rhiDeviceContext* context) override;
    void build_pipeline(renderShared* rs) override;
    void begin_barrier(rhiCommandList* cmd) override;
    void end_barrier(rhiCommandList* cmd) override;

private:
    rhiDescriptorSetLayout set_textures;
};