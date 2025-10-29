#pragma once

#include "indirectDrawPass.h"

class scene;
class camera;
class rhiTextureBindlessTable;

namespace shadowpass
{
	constexpr i32 aabb_corners_count = 8;
}

struct shadowInitContext : public drawInitContext
{
	scene* s;
	vec2 framebuffer_size;
	std::weak_ptr<rhiTextureBindlessTable> bindless_table;

	virtual std::unique_ptr<drawInitContext> clone() const
	{
		return std::make_unique<shadowInitContext>(*this);
	}
};

class shadowPass final : public indirectDrawPass
{
public:
	struct shadowCB
	{
		mat4 light_viewproj; // 64
		u32 cascade_index; // 4
		f32 _pad;
	};

	struct materialPC
	{
		u32 base_color_index;
		u32 base_sampler_index;
		f32 opacity_scale = 1.f;
		f32 alpha_cutoff = 0.f;
	};

public:
	void initialize(const drawInitContext& context) override;
	void render(renderShared* rs) override;
	void begin_barrier(rhiCommandList* cmd) override;
	void end_barrier(rhiCommandList* cmd) override;
	void update_instances(renderShared* rs, const u32 instancebuf_desc_idx) override;

public:
	void update(renderShared* rs, scene* s, const vec2 framebuffer_size);
	rhiTexture* get_shadow_texture() { return shadow_depth.get(); }
	std::vector<f32>& get_cascade_splits() { return cascade_splits; }
	const std::vector<mat4> get_light_viewproj();

protected:
	void build_layouts(renderShared* rs) override;
	void build_attachments(rhiDeviceContext* context) override;
	void build_pipeline(renderShared* rs) override;

private:
	void build(scene* s, const vec2 framebuffer_size);
	void update_cascade(scene* s, const vec2 framebuffer_size);

private:
	u32 cascade_count;
	rhiDescriptorSetLayout set_instances;

	std::vector<shadowCB> light_vps;

	std::unique_ptr<rhiTexture> shadow_depth;
	std::vector<f32> cascade_splits;

	// translucent
	rhiPipelineLayout opacity_pipe_layout;
	std::vector<std::vector<rhiDescriptorSet>> opacity_descriptor_sets;
	std::unique_ptr<rhiPipeline> opacity_pipeline;
};
