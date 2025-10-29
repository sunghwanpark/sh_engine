#pragma once

#include "pch.h"
#include "rhi/rhiDefs.h"
#include "renderer/renderShared.h"
#include "renderer/shadowPass.h"
#if MESHLET
#include "renderer/gbufferPass_meshlet.h"
#include "meshlet/meshletDef.h"
using namespace meshlet;
#else
#include "renderer/gbufferPass.h"
#endif
#include "renderer/skyPass.h"
#include "renderer/lightingPass.h"
#include "renderer/translucentPass.h"
#include "renderer/oitResolvePass.h"
#include "renderer/compositePass.h"
#include "textureCache.h"

class scene;
class rhiDeviceContext;
class rhiRenderResource;
class glTFMesh;
class rhiBuffer;
class rhiTexture;
class rhiFrameContext;
class rhiCommandList;
class rhiBindlessTable;
class meshActor;

class renderer
{
public:
	renderer();
	~renderer();

public:
	void initialize(scene* s, rhiDeviceContext* device_context, rhiFrameContext* frame_context);
	void pre_render(scene* s);
	void render(scene* s);
	void post_render();

private:
	struct drawGroupKey 
	{
		const rhiBuffer* vbo;
		const rhiBuffer* ibo;
		u32 base_color_index;
		u32 norm_color_index;
		u32 m_r_color_index;
		u32 base_sam_index;
		u32 norm_sam_index;
		u32 m_r_sam_index;

		f32 alpha_cutoff;
		f32 metalic_factor;
		f32 roughness_factor;
		bool is_transluent = false;
		bool is_double_sided = false;

		u32 first_index;
		u32 index_count;

		bool operator==(const drawGroupKey& o) const
		{
			return vbo == o.vbo
				&& ibo == o.ibo
				&& base_color_index == o.base_color_index
				&& norm_color_index == o.norm_color_index
				&& m_r_color_index == o.m_r_color_index
				&& base_sam_index == o.base_sam_index
				&& norm_sam_index == o.norm_sam_index
				&& m_r_sam_index == o.m_r_sam_index
				&& alpha_cutoff == o.alpha_cutoff
				&& metalic_factor == o.metalic_factor
				&& roughness_factor == o.roughness_factor
				&& is_transluent == o.is_transluent
				&& is_double_sided == o.is_double_sided
				&& first_index == o.first_index
				&& index_count == o.index_count;
		}
	};
	struct drawGroupKeyHash 
	{
		size_t operator()(const drawGroupKey& k) const noexcept 
		{
			size_t h = std::hash<const void*>()(k.vbo);
			h ^= (std::hash<const void*>()(k.ibo) << 1);
			h ^= (std::hash<u32>()(k.base_color_index) << 2);
			h ^= (std::hash<u32>()(k.norm_color_index) << 3);
			h ^= (std::hash<u32>()(k.m_r_color_index) << 4);
			h ^= (std::hash<u32>()(k.base_sam_index) << 5);
			h ^= (std::hash<u32>()(k.norm_sam_index) << 6);
			h ^= (std::hash<u32>()(k.m_r_sam_index) << 7);
			h ^= (std::hash<f32>()(k.alpha_cutoff) << 8);
			h ^= (std::hash<f32>()(k.metalic_factor) << 9);
			h ^= (std::hash<f32>()(k.roughness_factor) << 10);
			h ^= (std::hash<bool>()(k.is_transluent) << 11);
			h ^= (std::hash<bool>()(k.is_double_sided) << 12);
			h ^= (std::hash<u32>()(k.first_index) << 13);
			h ^= (std::hash<u32>()(k.index_count) << 14);
			return h;
		}
	};

	void prepare(scene* s);
#if MESHLET
	void build_meshlet(scene* s);
	void build_meshlet_drawcommand(scene* s);
	void build_meshlet_global_vertices(meshActor* a, meshlet::buildOut& out);
	void build_meshlet_ssbo(const meshlet::buildOut* out);
#else
	void build(scene* s, rhiDeviceContext* context);
#endif
	void notify_nextimage_index_to_drawpass(const u32 image_index);
	std::shared_ptr<rhiRenderResource> get_or_create_resource(const std::shared_ptr<glTFMesh> raw_mesh);
	void create_ringbuffer(const u32 frame_size);

private:
	std::unordered_map<u64, std::shared_ptr<rhiRenderResource>> cache;
	std::unique_ptr<textureCache> texture_cache;

	renderShared render_shared;
	shadowPass shadow_pass;
#if MESHLET
	gbufferPass_meshlet gbuffer_pass;
#else
	gbufferPass gbuffer_pass;
#endif
	skyPass sky_pass;
	lightingPass lighting_pass;
	translucentPass translucent_pass;
	oitResolvePass oit_pass;
	compositePass composite_pass;

	// indirect cpu data
	instanceArray instances;
	indirectArray indirect_args;
	groupRecordArray groups;

	drawTypeBuffers instance_buffer;
	drawTypeBuffers indirect_buffer;
	// end indirect cpu data

	// global ring buffer
	std::vector<std::unique_ptr<rhiBuffer>> global_ringbuffer;

	// actors bindless table
	std::shared_ptr<rhiTextureBindlessTable> bindless_table;
	
	// meshlet
	drawTypeBuffers meshlet_draw_buffer;
	meshletBuffer meshlet_ssbo;
	// end meshlet

	bool initialized = false;
	u32vec2 framebuffer_size = { 0, 0 };
};