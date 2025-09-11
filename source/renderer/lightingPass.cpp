#include "lightingPass.h"
#include "renderShared.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiFrameContext.h"
#include "rhi/rhiCommandList.h"
#include "scene/camera.h"

void lightingPass::initialize(const drawInitContext& context)
{
	drawPass::initialize(context);

	camera_cbuffer.resize(draw_context->rs->frame_context->get_frame_size());
	light_cbuffer.resize(draw_context->rs->frame_context->get_frame_size());
	for (u32 i = 0; i < draw_context->rs->frame_context->get_frame_size(); ++i)
	{
		draw_context->rs->create_or_resize_buffer(camera_cbuffer[i], sizeof(lightingPass::cam), rhiBufferUsage::uniform | rhiBufferUsage::transfer_dst, rhiMem::auto_device, 0);
		draw_context->rs->create_or_resize_buffer(light_cbuffer[i], sizeof(lightingPass::light), rhiBufferUsage::uniform | rhiBufferUsage::transfer_dst, rhiMem::auto_device, 0);
	}
}

void lightingPass::link_textures(context& context)
{
	scene_color = context.scene_color;
	gbuf_a = context.gbuf_a;
	gbuf_b = context.gbuf_b;
	gbuf_c = context.gbuf_c;
	depth = context.depth;
	shadows = context.shadows;

	render_info.color_attachments[0].view = {
		.texture = scene_color,
		.mip = 0,
		.layout = rhiImageLayout::color_attachment
	};
}

void lightingPass::update(renderShared* rs, rhiCommandList* cmd, camera* camera, const vec3& light_dir, const std::vector<mat4>& light_viewprojs, const std::vector<f32>& cascade_splits, const u32 shadow_resolution)
{ 
	update_cbuffers(rs, cmd, camera, light_dir, light_viewprojs, cascade_splits, shadow_resolution);
	update_descriptors(rs);
}

void lightingPass::update_cbuffers(renderShared* rs, rhiCommandList* cmd, camera* camera, const vec3& light_dir, const std::vector<mat4>& light_viewprojs, const std::vector<f32>& cascade_splits, const u32 shadow_resolution)
{
	cam c{
		.inv_proj = glm::inverse(camera->proj(vec2(draw_context->w, draw_context->h))),
		.inv_view = glm::inverse(camera->view()),
		.near_far = vec2(camera->get_near(), camera->get_far()),
		.cascade_splits = vec4(cascade_splits[0], cascade_splits[1], cascade_splits[2], cascade_splits[3]),
		.light_dir = light_dir,
		.shadow_mapsize = vec2(shadow_resolution, shadow_resolution),
	};
	light l;
	for (u32 i = 0; i < light_viewprojs.size(); ++i)
		l.light_viewproj[i] = light_viewprojs[i];

	// camera cbuffer
	cmd->buffer_barrier(camera_cbuffer[image_index.value()].get(), rhiPipelineStage::fragment_shader, rhiPipelineStage::copy, rhiAccessFlags::uniform_read, rhiAccessFlags::transfer_write, 0, sizeof(cam));
	rs->upload_to_device(cmd, camera_cbuffer[image_index.value()].get(), &c, sizeof(cam));
	cmd->buffer_barrier(camera_cbuffer[image_index.value()].get(), rhiPipelineStage::copy, rhiPipelineStage::fragment_shader, rhiAccessFlags::transfer_write, rhiAccessFlags::uniform_read, 0, sizeof(cam));

	// light cbuffer
	cmd->buffer_barrier(light_cbuffer[image_index.value()].get(), rhiPipelineStage::fragment_shader, rhiPipelineStage::copy, rhiAccessFlags::uniform_read, rhiAccessFlags::transfer_write, 0, sizeof(light));
	rs->upload_to_device(cmd, light_cbuffer[image_index.value()].get(), &l, sizeof(light));
	cmd->buffer_barrier(light_cbuffer[image_index.value()].get(), rhiPipelineStage::copy, rhiPipelineStage::fragment_shader, rhiAccessFlags::transfer_write, rhiAccessFlags::uniform_read, 0, sizeof(light));
}

void lightingPass::build_layouts(renderShared* rs)
{
	set_fragment_layout = rs->context->create_descriptor_set_layout(
		{
			rhiDescriptorSetLayoutBinding{
				.binding = 0,
				.type = rhiDescriptorType::uniform_buffer,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 1,
				.type = rhiDescriptorType::uniform_buffer,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 2,
				.type = rhiDescriptorType::sampled_image,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 3,
				.type = rhiDescriptorType::sampled_image,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 4,
				.type = rhiDescriptorType::sampled_image,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 5,
				.type = rhiDescriptorType::sampled_image,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 6,
				.type = rhiDescriptorType::sampler,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 7,
				.type = rhiDescriptorType::sampled_image,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 8,
				.type = rhiDescriptorType::sampler,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
		}, 0);
	create_pipeline_layout(rs, { set_fragment_layout }, 0);
	create_descriptor_sets(rs, { set_fragment_layout });
}

void lightingPass::build_attachments(rhiDeviceContext* context)
{
	render_info.samples = rhiSampleCount::x1;
	render_info.color_formats = { rhiFormat::RGBA8_UNORM };
	render_info.depth_format = std::nullopt;
	
	render_info.color_attachments.resize(1);
	render_info.color_attachments[0].load_op = rhiLoadOp::clear;
	render_info.color_attachments[0].store_op = rhiStoreOp::store;
	render_info.color_attachments[0].clear = { {0,0,0,1},1.0f,0 };
}

void lightingPass::build_pipeline(renderShared* rs)
{
	auto vs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\fullscreen.vs.spv");
	auto fs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\lighting.ps.spv");
	const rhiGraphicsPipelineDesc pipeline_desc{
		.vs = vs,
		.fs = fs,
		.color_formats = { rhiFormat::RGBA8_UNORM },
		.depth_format = std::nullopt,
		.samples = rhiSampleCount::x1,
		.depth_test = false,
		.depth_write = false
	};

	pipeline = rs->context->create_graphics_pipeline(pipeline_desc, pipeline_layout);

	shaderio::free_shader_binary(vs);
	shaderio::free_shader_binary(fs);
}

void lightingPass::update_descriptors(renderShared* rs)
{
	const rhiDescriptorBufferInfo cam{
		.buffer = camera_cbuffer[image_index.value()].get(),
		.offset = 0,
		.range = sizeof(lightingPass::cam)
	};
	const rhiWriteDescriptor cam_cb{
		.set = descriptor_sets[image_index.value()][0] ,
		.binding = 0,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::uniform_buffer,
		.buffer = { cam }
	};

	const rhiDescriptorBufferInfo light{
		.buffer = light_cbuffer[image_index.value()].get(),
		.offset = 0,
		.range = sizeof(lightingPass::light)
	};
	const rhiWriteDescriptor light_cb{
		.set = descriptor_sets[image_index.value()][0] ,
		.binding = 1,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::uniform_buffer,
		.buffer = { light }
	};

	auto mk_img = [&](rhiTexture* t)  -> rhiDescriptorImageInfo
		{
			return {
				.sampler = nullptr,
				.texture = t,
				.mip = 0,
				.base_layer = 0,
				.layer_count = 1,
				.layout = rhiImageLayout::shader_readonly
			};
		};

	const rhiWriteDescriptor gbuf_a_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 2,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampled_image,
		.image = { mk_img(gbuf_a)}
	};
	const rhiWriteDescriptor gbuf_b_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 3,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampled_image,
		.image = { mk_img(gbuf_b)}
	};
	const rhiWriteDescriptor gbuf_c_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 4,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampled_image,
		.image = { mk_img(gbuf_c)}
	};
	const rhiWriteDescriptor depth_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 5,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampled_image,
		.image = { rhiDescriptorImageInfo{
				.sampler = nullptr,
				.texture = depth,
				.mip = 0,
				.base_layer = 0,
				.layer_count = 1,
				.is_separate_depth_view = true,
				.layout = rhiImageLayout::shader_readonly
			}
		}
	};

	const rhiDescriptorImageInfo sampler_image_info{
		.sampler = rs->samplers.linear_clamp.get()
	};
	const rhiWriteDescriptor sampler_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 6,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampler,
		.image = { sampler_image_info }
	};

	const rhiWriteDescriptor shadow_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 7,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampled_image,
		.image = { rhiDescriptorImageInfo{
				.sampler = nullptr,
				.texture = shadows,
				.mip = 0,
				.base_layer = 0,
				.layer_count = shadows->desc.layers,
				.layout = rhiImageLayout::shader_readonly
			}
		}
	};

	const rhiDescriptorImageInfo shadow_sampler_image_info{
		.sampler = rs->samplers.point_clamp.get()
	};
	const rhiWriteDescriptor shadow_sampler_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 8,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampler,
		.image = { shadow_sampler_image_info }
	};

	rs->context->update_descriptors({ 
		cam_cb,
		light_cb,
		gbuf_a_write_desc,
		gbuf_b_write_desc,
		gbuf_c_write_desc,
		depth_write_desc,
		sampler_write_desc,
		shadow_write_desc,
		shadow_sampler_write_desc
		});
}

void lightingPass::begin_barrier(rhiCommandList* cmd)
{
	if (is_first_frame)
	{
		cmd->image_barrier(scene_color, rhiImageLayout::undefined, rhiImageLayout::color_attachment);
		is_first_frame = false;
	}
	else
	{
		cmd->image_barrier(scene_color, rhiImageLayout::shader_readonly, rhiImageLayout::color_attachment);
	}
}

void lightingPass::end_barrier(rhiCommandList* cmd)
{
	cmd->image_barrier(scene_color, rhiImageLayout::color_attachment, rhiImageLayout::shader_readonly);
}

void lightingPass::draw(rhiCommandList* cmd)
{
	cmd->draw_fullscreen();
}