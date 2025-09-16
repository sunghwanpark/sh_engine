#pragma once

#include "indirectDrawPass.h"

class scene;
class camera;

namespace shadowpass
{
	constexpr i32 aabb_corners_count = 8;
}

struct shadowInitContext : public drawInitContext
{
	scene* s;
	vec2 framebuffer_size;

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
		mat4 light_viewproj;
		u32 cascade_index;
	};

public:
	void initialize(const drawInitContext& context) override;
	void render(renderShared* rs) override;
	void begin_barrier(rhiCommandList* cmd) override;
	void end_barrier(rhiCommandList* cmd) override;

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
	void update_globals(renderShared* rs, const u32 current_cascade);
	void update_cascade(scene* s, const vec2 framebuffer_size);

private:
	u32 cascade_count;
	rhiDescriptorSetLayout set_instances;

	std::vector<shadowCB> light_vps;
	std::vector<std::unique_ptr<rhiBuffer>> uniform_ring_buffer;

	std::unique_ptr<rhiTexture> shadow_depth;
	std::vector<f32> cascade_splits;
};
