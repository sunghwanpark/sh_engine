#pragma once

#include "meshletDrawPass.h"

class rhiTextureBindlessTable;
class rhiDeviceContext;
struct rhiBindlessHandle;

struct gbufferPass_meshletInitContext : public drawInitContext
{
    std::weak_ptr<rhiTextureBindlessTable> bindless_table;
    std::unique_ptr<drawInitContext> clone() const override
    {
        return std::make_unique<gbufferPass_meshletInitContext>(*this);
    }
};

class gbufferPass_meshlet final : public meshletDrawPass
{
public:
    struct alignas(16) drawParamIndex
    {
        u32 dp_index; // 4b
        u32 __pad[3]; // 12b
    }; // 16b

public:
    void initialize(const drawInitContext& context) override;
    void begin(rhiCommandList* cmd) override;
    void draw(rhiCommandList* cmd) override;
    void begin_barrier(rhiCommandList* cmd) override;
    void end_barrier(rhiCommandList* cmd) override;

public:
    rhiTexture* get_gbuffer_a() const { return gbuffer_a.get(); }
    rhiTexture* get_gbuffer_b() const { return gbuffer_b.get(); }
    rhiTexture* get_gbuffer_c() const { return gbuffer_c.get(); }
    rhiTexture* get_depth() const { return depth.get(); }

protected:
    void build_layouts(renderShared* rs) override;
    void build_attachments(rhiDeviceContext* context) override;
    void build_pipeline(renderShared* rs) override;

private:
    std::unique_ptr<rhiTexture> gbuffer_a;
    std::unique_ptr<rhiTexture> gbuffer_b;
    std::unique_ptr<rhiTexture> gbuffer_c;
    std::unique_ptr<rhiTexture> depth;

    rhiDescriptorSetLayout layout_globals;
    rhiDescriptorSetLayout layout_material;
};
