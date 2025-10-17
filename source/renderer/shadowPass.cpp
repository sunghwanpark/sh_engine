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
	init_context = context.clone();

	{
		auto ptr = static_cast<shadowInitContext*>(init_context.get());
		ASSERT(ptr);
		build(ptr->s, ptr->framebuffer_size);
	}

	// constant size
	init_context->w = 2048;
	init_context->h = 2048;
	render_info.samples = rhiSampleCount::x1;
	render_info.render_area = { 0, 0, init_context->w, init_context->h };

	const rhiTextureDesc td_depth
	{
		.width = init_context->w,
		.height = init_context->h,
		.layers = cascade_count,
		.mips = 1,
		.format = rhiFormat::D32F,
		.samples = rhiSampleCount::x1,
		.usage = rhiTextureUsage::ds,
		.is_depth = true
	};
	{
		auto tex = init_context->rs->context->create_texture(td_depth);
		shadow_depth.reset(tex.release());
	}

	descriptor_sets.resize(init_context->rs->frame_context->get_frame_size());
	build_layouts(init_context->rs);
	build_attachments(init_context->rs->context);
	build_pipeline(init_context->rs);
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

	const f32 n = s->get_camera()->get_near();
	const f32 f = s->get_camera()->get_far();
	const f32 range = f - n;
	const f32 ratio = f / n;

	for (u32 i = 0; i < cascade_count; i++)
	{
		f32 p = (i + 1) / static_cast<f32>(cascade_count);
		f32 log = n * std::pow(ratio, p);
		f32 uni = n + range * p;
		f32 d = 0.6f * (log - uni) + uni;
		cascade_splits[i] = (d - n) / range;
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

void shadowPass::build_layouts(renderShared* rs)
{
	// default
	set_instances = rs->context->create_descriptor_set_layout(
		{
			{
				.binding = 0,
				.type = rhiDescriptorType::storage_buffer,
				.count = 1,
				.stage = rhiShaderStage::vertex
			}
		}, 0);
	create_pipeline_layout(rs, { set_instances }, { { rhiShaderStage::vertex, sizeof(shadowPass::shadowCB) } });
	create_descriptor_sets(rs, { set_instances });

	// opacity
	auto ptr = static_cast<shadowInitContext*>(init_context.get());
	ASSERT(ptr);
	
	auto table_ptr = ptr->bindless_table.lock();
	ASSERT(table_ptr);
	opacity_pipe_layout = rs->context->create_pipeline_layout({ set_instances, table_ptr->get_set_layout() }, {
		rhiPushConstant{
			.stage = rhiShaderStage::vertex,
			.bytes = sizeof(shadowPass::shadowCB)
			},
		rhiPushConstant{
			.stage = rhiShaderStage::fragment,
			.bytes = sizeof(shadowPass::materialPC)
			}}, nullptr);
	const u32 frame_size = rs->get_frame_size();
	opacity_descriptor_sets.resize(frame_size);
	for (u32 index = 0; index < frame_size; ++index)
	{
		auto& desc_pool = rs->arena.pools[index];
		opacity_descriptor_sets[index] = rs->context->allocate_descriptor_sets(desc_pool, { set_instances });
	}
}

void shadowPass::build_attachments(rhiDeviceContext* context)
{
	render_info.renderpass_name = "shadow";
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
	// default
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
		shaderio::free_shader_binary(fs);
	}
	// opacity
	{
		auto vs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\shadow_opacity.vs.spv");
		auto fs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\shadow_opacity.ps.spv");
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
					rhiVertexAttributeDesc{ 0, 0, rhiFormat::RGB32_SFLOAT, offsetof(glTFVertex, position) },
					rhiVertexAttributeDesc{ 1, 0, rhiFormat::RG32_SFLOAT, offsetof(glTFVertex, uv) }
			}}
		};
		opacity_pipeline = rs->context->create_graphics_pipeline(desc, opacity_pipe_layout);
		shaderio::free_shader_binary(vs);
		shaderio::free_shader_binary(fs);
	}
}

void shadowPass::update_instances(renderShared* rs, const u32 instancebuf_desc_idx)
{
	// default
	{
		auto& buf = instance_buffer->at(static_cast<u8>(drawType::gbuffer));
		if (buf)
		{
			const rhiDescriptorBufferInfo buffer_info{
			.buffer = buf.get(),
			.offset = 0,
			.range = buf->size()
			};
			const rhiWriteDescriptor write_desc{
				.set = descriptor_sets[image_index.value()][instancebuf_desc_idx],
				.binding = 0,
				.array_index = 0,
				.count = 1,
				.type = rhiDescriptorType::storage_buffer,
				.buffer = { buffer_info }
			};
			rs->context->update_descriptors({ write_desc });
		}
	}
	// opacity
	{
		auto& buf = instance_buffer->at(static_cast<u8>(drawType::translucent));
		if (buf)
		{
			const rhiDescriptorBufferInfo buffer_info{
			.buffer = buf.get(),
			.offset = 0,
			.range = buf->size()
			};
			const rhiWriteDescriptor write_desc{
				.set = opacity_descriptor_sets[image_index.value()][instancebuf_desc_idx],
				.binding = 0,
				.array_index = 0,
				.count = 1,
				.type = rhiDescriptorType::storage_buffer,
				.buffer = { buffer_info }
			};
			rs->context->update_descriptors({ write_desc });
		}
	}
}

void shadowPass::render(renderShared* rs)
{
	auto cmd = rs->frame_context->get_command_list(main_job_queue);

	begin_barrier(cmd);
	cmd->begin_render_pass(render_info);
	cmd->set_viewport_scissor(vec2(init_context->w, init_context->h));
	constexpr u32 stride = sizeof(rhiDrawIndexedIndirect);
	// default
	{
		cmd->bind_pipeline(pipeline.get());
		cmd->bind_descriptor_sets(pipeline_layout, rhiPipelineType::graphics, descriptor_sets[image_index.value()], 0, dynamic_offsets);

		auto buffer_ptr = indirect_buffer->at(static_cast<u8>(drawType::gbuffer));
		for (u32 index = 0; index < cascade_count; ++index)
		{
			cmd->push_constants(pipeline_layout, rhiShaderStage::vertex, 0, sizeof(shadowCB), &light_vps[index]);
			auto gbuffer_group = group_records->at(static_cast<u8>(drawType::gbuffer));
			for (const auto& g : gbuffer_group)
			{
				cmd->bind_vertex_buffer(const_cast<rhiBuffer*>(g.vbo), 0, 0);
				cmd->bind_index_buffer(const_cast<rhiBuffer*>(g.ibo), 0);
				const u32 byte_offset = g.first_cmd * stride;
				cmd->draw_indexed_indirect(buffer_ptr.get(), byte_offset, g.cmd_count, stride);
			}
		}
	}
	// opacity
	{
		auto buffer_ptr = indirect_buffer->at(static_cast<u8>(drawType::translucent));
		if (buffer_ptr)
		{
			cmd->bind_pipeline(opacity_pipeline.get());
			cmd->bind_descriptor_sets(opacity_pipe_layout, rhiPipelineType::graphics, opacity_descriptor_sets[image_index.value()], 0, dynamic_offsets);
			auto ptr = static_cast<shadowInitContext*>(init_context.get());
			ASSERT(ptr);
		
			auto table_ptr = ptr->bindless_table.lock();
			ASSERT(table_ptr);
			table_ptr->bind_once(cmd, opacity_pipe_layout, 1);
		
			for (u32 index = 0; index < cascade_count; ++index)
			{
				cmd->push_constants(opacity_pipe_layout, rhiShaderStage::vertex, 0, sizeof(shadowCB), &light_vps[index]);
				auto gbuffer_group = group_records->at(static_cast<u8>(drawType::translucent));
				for (const auto& g : gbuffer_group)
				{
					const shadowPass::materialPC mat{
						.base_color_index = g.base_color_index,
						.base_sampler_index = g.base_sam_index
					};
					cmd->push_constants(opacity_pipe_layout, rhiShaderStage::fragment, sizeof(shadowCB), sizeof(shadowPass::materialPC), &mat);
					cmd->bind_vertex_buffer(const_cast<rhiBuffer*>(g.vbo), 0, 0);
					cmd->bind_index_buffer(const_cast<rhiBuffer*>(g.ibo), 0);
					const u32 byte_offset = g.first_cmd * stride;
					cmd->draw_indexed_indirect(buffer_ptr.get(), byte_offset, g.cmd_count, stride);
				}
			}
		}
	}

	cmd->end_render_pass();
	end_barrier(cmd);
	image_index.reset();
}

void shadowPass::begin_barrier(rhiCommandList* cmd)
{
	if (is_first_frame)
	{
		cmd->image_barrier(shadow_depth.get(), rhiImageLayout::undefined, rhiImageLayout::depth_stencil_attachment, 0, 1, 0, cascade_count);
		is_first_frame = false;
	}
	else// SRV -> RTV
	{
		cmd->image_barrier(shadow_depth.get(), rhiImageLayout::shader_readonly, rhiImageLayout::depth_stencil_attachment, 0, 1, 0, cascade_count);
	}
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