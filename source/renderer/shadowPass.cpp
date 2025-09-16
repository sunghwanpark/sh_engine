#include "shadowPass.h"
#include "scene/scene.h"
#include "scene/camera.h"
#include "scene/light/directionalLightActor.h"
#include "mesh/glTFMesh.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiDescriptor.h"
#include "rhi/rhiCommandList.h"
#include "rhi/rhiFrameContext.h"

void shadowPass::initialize(const drawInitContext& context)
{
	draw_context = context.clone();

	{
		auto ptr = static_cast<shadowInitContext*>(draw_context.get());
		assert(ptr);
		build(ptr->s, ptr->framebuffer_size);
	}

	// constant size
	draw_context->w = 2048;
	draw_context->h = 2048;
	render_info.samples = rhiSampleCount::x1;
	render_info.render_area = { 0, 0, draw_context->w, draw_context->h };

	const rhiTextureDesc td_depth
	{
		.width = draw_context->w,
		.height = draw_context->h,
		.layers = cascade_count,
		.mips = 1,
		.format = rhiFormat::D32F,
		.samples = rhiSampleCount::x1,
		.is_depth = true
	};
	{
		auto tex = draw_context->rs->context->create_texture(td_depth);
		shadow_depth.reset(tex.release());
	}

	descriptor_sets.resize(draw_context->rs->frame_context->get_frame_size());
	build_layouts(draw_context->rs);
	build_attachments(draw_context->rs->context);
	build_pipeline(draw_context->rs);

	uniform_ring_buffer.resize(draw_context->rs->get_frame_size());
	std::ranges::for_each(uniform_ring_buffer, [&](std::unique_ptr<rhiBuffer>& buf)
		{
			draw_context->rs->create_or_resize_buffer(buf, sizeof(shadowCB), rhiBufferUsage::uniform | rhiBufferUsage::transfer_dst, rhiMem::auto_device, sizeof(shadowCB));
		});
}

void shadowPass::build(scene* s, const vec2 framebuffer_size)
{
	cascade_count = s->get_directional_light()->get_cascade_count();
	light_vps.resize(cascade_count);
}

void shadowPass::update(renderShared* rs, scene* s, const vec2 framebuffer_size)
{
	update_cascade(s, framebuffer_size);
	update_instances(rs, 0);
}

void shadowPass::update_cascade(scene* s, const vec2 framebuffer_size)
{
	if (cascade_splits.empty())
		cascade_splits.resize(cascade_count);

	const f32 near = s->get_camera()->get_near();
	const f32 far = s->get_camera()->get_far();
	const f32 range = far - near;
	const f32 ratio = far / near;

	for (u32 i = 0; i < cascade_count; i++)
	{
		f32 p = (i + 1) / static_cast<f32>(cascade_count);
		f32 log = near * std::pow(ratio, p);
		f32 uni = near + range * p;
		f32 d = 0.6f * (log - uni) + uni;
		cascade_splits[i] = (d - near) / range;
	}

	const mat4 v = s->get_camera()->view();
	const mat4 p = s->get_camera()->proj(framebuffer_size);
	const mat4 inv_cam = glm::inverse(p * v);
	const vec3 light_dir = normalize(s->get_directional_light()->get_direction());

	f32 last_split_dist = 0.0;
	for (u32 i = 0; i < cascade_count; ++i)
	{
		std::array<vec3, shadowpass::aabb_corners_count> frustum_corners = {
			vec3(-1.0f,  1.0f, 0.0f),
			vec3(1.0f,  1.0f, 0.0f),
			vec3(1.0f, -1.0f, 0.0f),
			vec3(-1.0f, -1.0f, 0.0f),
			vec3(-1.0f,  1.0f,  1.0f),
			vec3(1.0f,  1.0f,  1.0f),
			vec3(1.0f, -1.0f,  1.0f),
			vec3(-1.0f, -1.0f,  1.0f),
		};

		f32 split_dist = cascade_splits[i];
		for (u32 j = 0; j < shadowpass::aabb_corners_count; ++j)
		{
			vec4 inv_corner = inv_cam * vec4(frustum_corners[j], 1.0f);
			frustum_corners[j] = inv_corner / inv_corner.w;
		}
		for (u32 j = 0; j < 4; ++j)
		{
			vec3 dist = frustum_corners[j + 4] - frustum_corners[j];
			frustum_corners[j + 4] = frustum_corners[j] + (dist * split_dist);
			frustum_corners[j] = frustum_corners[j] + (dist * last_split_dist);
		}

		vec3 frustum_center = glm::vec3(0.0f);
		for (u32 j = 0; j < shadowpass::aabb_corners_count; ++j)
		{
			frustum_center += frustum_corners[j];
		}
		frustum_center /= static_cast<f32>(shadowpass::aabb_corners_count);

		mat4 light_view = glm::lookAtLH(frustum_center - light_dir * 100.0f, frustum_center, glm::vec3(0.0f, 1.0f, 0.0f));

		vec3 min_extents = vec3(std::numeric_limits<float>::max());
		vec3 max_extents = vec3(std::numeric_limits<float>::lowest());

		for (u32 j = 0; j < shadowpass::aabb_corners_count; ++j)
		{
			vec4 light_space_corner = light_view * vec4(frustum_corners[j], 1.0f);
			min_extents = glm::min(min_extents, vec3(light_space_corner));
			max_extents = glm::max(max_extents, vec3(light_space_corner));
		}

		mat4 light_orthoproj = glm::orthoLH(min_extents.x, max_extents.x, min_extents.y, max_extents.y, 0.0f, max_extents.z);

		light_vps[i].light_viewproj = light_orthoproj * light_view;
		light_vps[i].cascade_index = i;
		last_split_dist = cascade_splits[i];
	}
}

/*void shadowPass::update_cascade(scene* s, const vec2 framebuffer_size)
{
	if(cascade_splits.empty())
		cascade_splits.resize(cascade_count);

	const f32 near = s->get_camera()->get_near();
	const f32 far = s->get_camera()->get_far();
	const f32 range = far - near;
	const f32 ratio = far / near;

	for (u32 i = 0; i < cascade_count; i++)
	{
		f32 p = (i + 1) / static_cast<f32>(cascade_count);
		f32 log = near * std::pow(ratio, p);
		f32 uni = near + range * p;
		f32 d = 0.6f * (log - uni) + uni;
		cascade_splits[i] = (d - near) / range;
	}
	
	const mat4 v = s->get_camera()->view();
	const mat4 p = s->get_camera()->proj(framebuffer_size);
	const mat4 inv_cam = glm::inverse(p * v);
	const vec3 light_dir = normalize(s->get_directional_light()->get_direction());

	f32 last_split_dist = 0.0;
	for (u32 i = 0; i < cascade_count; ++i) 
	{
		std::array<vec3, shadowpass::aabb_corners_count> frustum_corners = {
			vec3(-1.0f,  1.0f, 0.0f),
			vec3(1.0f,  1.0f, 0.0f),
			vec3(1.0f, -1.0f, 0.0f),
			vec3(-1.0f, -1.0f, 0.0f),
			vec3(-1.0f,  1.0f,  1.0f),
			vec3(1.0f,  1.0f,  1.0f),
			vec3(1.0f, -1.0f,  1.0f),
			vec3(-1.0f, -1.0f,  1.0f),
		};

		float split_dist = cascade_splits[i];
		// Project frustum corners into world space
		for (u32 j = 0; j < shadowpass::aabb_corners_count; ++j) 
		{
			vec4 inv_corner = inv_cam * vec4(frustum_corners[j], 1.0f);
			frustum_corners[j] = inv_corner / inv_corner.w;
		}

		for (u32 j = 0; j < 4; ++j) 
		{
			vec3 dist = frustum_corners[j + 4] - frustum_corners[j];
			frustum_corners[j + 4] = frustum_corners[j] + (dist * split_dist);
			frustum_corners[j] = frustum_corners[j] + (dist * last_split_dist);
		}

		// Get frustum center
		vec3 frustum_center = glm::vec3(0.0f);
		for (u32 j = 0; j < shadowpass::aabb_corners_count; ++j)
		{
			frustum_center += frustum_corners[j];
		}
		frustum_center /= static_cast<f32>(shadowpass::aabb_corners_count);

		f32 radius = 0.0f;
		for (u32 j = 0; j < shadowpass::aabb_corners_count; j++)
		{
			float distance = glm::length(frustum_corners[j] - frustum_center);
			radius = glm::max(radius, distance);
		}
		radius = std::ceil(radius * 16.0f) / 16.0f;

		const f32 z_padding = 10.0f;
		const f32 dist = radius * 2.0f + z_padding;
		mat4 light_view = glm::lookAtLH(frustum_center - light_dir, frustum_center, vec3(0, 1, 0));
		vec3 min_extents = vec3(std::numeric_limits<float>::max());
		vec3 max_extents = vec3(std::numeric_limits<float>::lowest());
		for (u32 j = 0; j < shadowpass::aabb_corners_count; ++j)
		{
			vec3 ls = light_view * vec4(frustum_corners[j], 1.f);
			min_extents = glm::min(min_extents, ls);
			max_extents = glm::max(max_extents, ls);
		}

		/*const f32 padding_xy_abs = 2.0f;
		const f32 padding_xy_relative = 0.03f * glm::max(max_extents.x - min_extents.x, max_extents.y - min_extents.y);
		const f32 padding_xy = glm::max(padding_xy_abs, padding_xy_relative);

		const f32 padding_z_abs = 5.0f;
		const f32 padding_z_relative = 0.05f * (max_extents.z - min_extents.z);
		const f32 padding_z = glm::max(padding_z_abs, padding_z_relative);

		min_extents.x -= padding_xy;
		min_extents.y -= padding_xy;
		min_extents.z -= padding_z;

		max_extents.x += padding_xy;
		max_extents.y += padding_xy;
		max_extents.z += padding_z;

		mat4 light_orthoproj = glm::orthoLH(min_extents.x, max_extents.x, min_extents.y, max_extents.y, min_extents.z, max_extents.z);

		light_vps[i].light_viewproj = light_orthoproj * light_view;
		light_vps[i].cascade_index = i;

		last_split_dist = cascade_splits[i];
	}
}*/

void shadowPass::build_layouts(renderShared* rs)
{
	set_instances = rs->context->create_descriptor_set_layout(
		{
			{
				.binding = 0,
				.type = rhiDescriptorType::storage_buffer,
				.count = 1,
				.stage = rhiShaderStage::vertex
			},
		}, 0);
	create_pipeline_layout(rs, { set_instances }, sizeof(shadowCB));
	create_descriptor_sets(rs, { set_instances });
}

void shadowPass::build_attachments(rhiDeviceContext* context)
{
	render_info.depth_format = rhiFormat::D32F;
	render_info.layer_count = cascade_count;

	rhiRenderTargetView d{
		.texture = shadow_depth.get(),
		.mip = 0,
		.base_layer = 0,
		.layer_count = cascade_count,
		.layout = rhiImageLayout::depth_stencil_attachment,
	};

	rhiRenderingAttachment depth_attachment;
	depth_attachment.view = d;
	depth_attachment.load_op = rhiLoadOp::clear;
	depth_attachment.store_op = rhiStoreOp::store;
	depth_attachment.clear.depth = 1.f;
	depth_attachment.clear.stencil = 0;
	render_info.depth_attachment = depth_attachment;
}

void shadowPass::build_pipeline(renderShared* rs)
{
	auto vs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\shadow.vs.spv");
	auto fs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\shadow.ps.spv");
	const rhiGraphicsPipelineDesc desc{
		.vs = vs,
		.fs = fs,
		.depth_format = shadow_depth->desc.format,
		.samples = rhiSampleCount::x1,
		.depth_test = true,
		.depth_write = true,
		.vertex_layout = rhiVertexAttribute{
			.binding = 0,
			.stride = sizeof(glTFVertex),
			.vertex_attr_desc = {
				rhiVertexAttributeDesc{ 0, 0, rhiFormat::RGB32_SFLOAT, offsetof(glTFVertex, position) }
		}}
	};
	pipeline = rs->context->create_graphics_pipeline(desc, pipeline_layout);
	shaderio::free_shader_binary(vs);
}

void shadowPass::update_globals(renderShared* rs, const u32 current_cascade)
{
	if (!uniform_ring_buffer[image_index.value()])
		assert(false);

	auto& current_frame = rs->frame_context->get(image_index.value());
	auto cmd_list = current_frame.cmd.get();

	cmd_list->buffer_barrier(uniform_ring_buffer[image_index.value()].get(), rhiPipelineStage::vertex_shader, rhiPipelineStage::copy, rhiAccessFlags::uniform_read, rhiAccessFlags::transfer_write, 0, sizeof(globalsCB));
	rs->upload_to_device(cmd_list, uniform_ring_buffer[image_index.value()].get(), &light_vps[current_cascade], sizeof(globalsCB));
	cmd_list->buffer_barrier(uniform_ring_buffer[image_index.value()].get(), rhiPipelineStage::copy, rhiPipelineStage::vertex_shader, rhiAccessFlags::transfer_write, rhiAccessFlags::uniform_read, 0, sizeof(globalsCB));

	const rhiDescriptorBufferInfo buffer_info{
		.buffer = uniform_ring_buffer[image_index.value()].get(),
		.offset = 0,
		.range = sizeof(shadowCB)
	};
	const rhiWriteDescriptor write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 0,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::uniform_buffer,
		.buffer = { buffer_info }
	};
	rs->context->update_descriptors({ write_desc });
}

void shadowPass::render(renderShared* rs)
{
	auto& current_frame = rs->frame_context->get(image_index.value());
	auto cmd = current_frame.cmd.get();

	begin_barrier(cmd);
	cmd->begin_render_pass(render_info);
	cmd->bind_pipeline(pipeline.get());
	cmd->bind_descriptor_sets(pipeline_layout, rhiPipelineType::graphics, descriptor_sets[image_index.value()], 0, dynamic_offsets);
	cmd->set_viewport_scissor(vec2(draw_context->w, draw_context->h));

	auto buffer_ptr = indirect_buffer.lock();
	constexpr u32 stride = sizeof(rhiDrawIndexedIndirect);
	for (u32 index = 0; index < cascade_count; ++index)
	{
		cmd->push_constants(pipeline_layout, rhiShaderStage::vertex, 0, sizeof(shadowCB), &light_vps[index]);
		for (const auto& g : *group_records)
		{
			cmd->bind_vertex_buffer(const_cast<rhiBuffer*>(g.vbo), 0, 0);
			cmd->bind_index_buffer(const_cast<rhiBuffer*>(g.ibo), 0);
			const u32 byte_offset = g.first_cmd * stride;
			cmd->draw_indexed_indirect(buffer_ptr.get(), byte_offset, g.cmd_count, stride);
		}
	}

	cmd->end_render_pass();
	end_barrier(cmd);
	image_index.reset();
}

void shadowPass::begin_barrier(rhiCommandList* cmd)
{
	// SRV -> RTV
	cmd->image_barrier(shadow_depth.get(), rhiImageLayout::shader_readonly, rhiImageLayout::depth_stencil_attachment , 0, 1, 0, cascade_count);
}

void shadowPass::end_barrier(rhiCommandList* cmd)
{
	// RTV -> SRV
	cmd->image_barrier(shadow_depth.get(), rhiImageLayout::depth_stencil_attachment, rhiImageLayout::shader_readonly, 0, 1, 0, cascade_count);
}

const std::vector<mat4> shadowPass::get_light_viewproj()
{
	std::vector<mat4> mats;
	for (const auto& cb : light_vps)
	{
		mats.push_back(cb.light_viewproj);
	}
	return mats;
}