#include "lightingPass.h"
#include "renderShared.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiFrameContext.h"
#include "rhi/rhiCommandList.h"
#include "scene/camera.h"

void lightingPass::initialize(const drawInitContext& context)
{
	drawPass::initialize(context);

	camera_cbuffer.resize(init_context->rs->frame_context->get_frame_size());
	light_cbuffer.resize(init_context->rs->frame_context->get_frame_size());
	ibl_param_cbuffer.resize(init_context->rs->frame_context->get_frame_size());
	for (u32 i = 0; i < init_context->rs->frame_context->get_frame_size(); ++i)
	{
		init_context->rs->create_or_resize_buffer(camera_cbuffer[i], sizeof(lightingPass::cam), rhiBufferUsage::uniform | rhiBufferUsage::transfer_dst, rhiMem::auto_device, 0);
		init_context->rs->create_or_resize_buffer(light_cbuffer[i], sizeof(lightingPass::light), rhiBufferUsage::uniform | rhiBufferUsage::transfer_dst, rhiMem::auto_device, 0);
		init_context->rs->create_or_resize_buffer(ibl_param_cbuffer[i], sizeof(lightingPass::light), rhiBufferUsage::uniform | rhiBufferUsage::transfer_dst, rhiMem::auto_device, 0);
	}
}

void lightingPass::link_textures(textureContext& context)
{
	texture_context.scene_color = context.scene_color;
	texture_context.gbuf_a = context.gbuf_a;
	texture_context.gbuf_b = context.gbuf_b;
	texture_context.gbuf_c = context.gbuf_c;
	texture_context.depth = context.depth;
	texture_context.shadows = context.shadows;
	texture_context.ibl_irradiance = context.ibl_irradiance;
	texture_context.ibl_specular = context.ibl_specular;
	texture_context.ibl_brdf_lut = context.ibl_brdf_lut;

	render_info.color_attachments[0].view = {
		.texture = texture_context.scene_color,
		.mip = 0,
		.layout = rhiImageLayout::color_attachment
	};
}

void lightingPass::update(renderShared* rs, camera* camera, const vec3& light_dir, const std::vector<mat4>& light_viewprojs, const std::vector<f32>& cascade_splits, const u32 shadow_resolution, const u32 cubemap_mipcount)
{ 
	update_cbuffers(rs, camera, light_dir, light_viewprojs, cascade_splits, shadow_resolution, cubemap_mipcount);
	update_descriptors(rs);
}

void lightingPass::update_cbuffers(renderShared* rs, camera* camera, const vec3& light_dir, const std::vector<mat4>& light_viewprojs, const std::vector<f32>& cascade_splits, const u32 shadow_resolution, const u32 cubemap_mipcount)
{
	cam c{
		.inv_proj = glm::inverse(camera->proj(vec2(init_context->w, init_context->h))),
		.inv_view = glm::inverse(camera->view()),
		.near_far = vec2(camera->get_near(), camera->get_far()),
		.cascade_splits = vec4(cascade_splits[0], cascade_splits[1], cascade_splits[2], cascade_splits[3]),
		.light_dir = light_dir,
		.shadow_mapsize = vec2(shadow_resolution, shadow_resolution),
	};

	light l;
	for (u32 i = 0; i < light_viewprojs.size(); ++i)
		l.light_viewproj[i] = light_viewprojs[i];

	const iblParams ibl{
		.ibl_intensity_diffuse = 0.5f,
		.ibl_intensity_specular = 0.1f,
		.specular_mip_count = static_cast<f32>(cubemap_mipcount)
	};

	// camera cbuffer
	rs->buffer_barrier(camera_cbuffer[image_index.value()].get(), rhiBufferBarrierDescription{
		rhiPipelineStage::fragment_shader, rhiPipelineStage::copy, rhiAccessFlags::uniform_read, rhiAccessFlags::transfer_write, 0, sizeof(cam), 
		rs->context->get_queue_family_index(rhiQueueType::graphics), rs->context->get_queue_family_index(rhiQueueType::transfer)});
	rs->upload_to_device(camera_cbuffer[image_index.value()].get(), &c, sizeof(cam));
	rs->buffer_barrier(camera_cbuffer[image_index.value()].get(), rhiBufferBarrierDescription{
		rhiPipelineStage::copy, rhiPipelineStage::fragment_shader, rhiAccessFlags::transfer_write, rhiAccessFlags::uniform_read, 0, sizeof(cam),
		rs->context->get_queue_family_index(rhiQueueType::graphics), rs->context->get_queue_family_index(rhiQueueType::transfer) });

	// light cbuffer
	rs->buffer_barrier(light_cbuffer[image_index.value()].get(), rhiBufferBarrierDescription{ 
		rhiPipelineStage::fragment_shader, rhiPipelineStage::copy, rhiAccessFlags::uniform_read, rhiAccessFlags::transfer_write, 0, sizeof(light),
		rs->context->get_queue_family_index(rhiQueueType::graphics), rs->context->get_queue_family_index(rhiQueueType::transfer) });
	rs->upload_to_device(light_cbuffer[image_index.value()].get(), &l, sizeof(light));
	rs->buffer_barrier(light_cbuffer[image_index.value()].get(), rhiBufferBarrierDescription{
		rhiPipelineStage::copy, rhiPipelineStage::fragment_shader, rhiAccessFlags::transfer_write, rhiAccessFlags::uniform_read, 0, sizeof(light),
		rs->context->get_queue_family_index(rhiQueueType::graphics), rs->context->get_queue_family_index(rhiQueueType::transfer) });

	// ibl cbuffer
	rs->buffer_barrier(ibl_param_cbuffer[image_index.value()].get(), rhiBufferBarrierDescription{ 
		rhiPipelineStage::fragment_shader, rhiPipelineStage::copy, rhiAccessFlags::uniform_read, rhiAccessFlags::transfer_write, 0, sizeof(iblParams),
		rs->context->get_queue_family_index(rhiQueueType::graphics), rs->context->get_queue_family_index(rhiQueueType::transfer) });
	rs->upload_to_device(ibl_param_cbuffer[image_index.value()].get(), &ibl, sizeof(iblParams));
	rs->buffer_barrier(ibl_param_cbuffer[image_index.value()].get(), rhiBufferBarrierDescription{
		rhiPipelineStage::copy, rhiPipelineStage::fragment_shader, rhiAccessFlags::transfer_write, rhiAccessFlags::uniform_read, 0, sizeof(iblParams),
		rs->context->get_queue_family_index(rhiQueueType::graphics), rs->context->get_queue_family_index(rhiQueueType::transfer) });
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
				.type = rhiDescriptorType::uniform_buffer,
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
				.type = rhiDescriptorType::sampled_image,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 7,
				.type = rhiDescriptorType::sampler,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 8,
				.type = rhiDescriptorType::sampled_image,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 9,
				.type = rhiDescriptorType::sampler,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 10,
				.type = rhiDescriptorType::sampled_image,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 11,
				.type = rhiDescriptorType::sampled_image,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 12,
				.type = rhiDescriptorType::sampled_image,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
		}, 0);
	create_pipeline_layout(rs, { set_fragment_layout });
	create_descriptor_sets(rs, { set_fragment_layout });
}

void lightingPass::build_attachments(rhiDeviceContext* context)
{
	render_info.renderpass_name = "lighting";
	render_info.samples = rhiSampleCount::x1;
	render_info.color_formats = { rhiFormat::RGBA8_UNORM };
	render_info.depth_format = std::nullopt;
	
	render_info.color_attachments.resize(1);
	render_info.color_attachments[0].load_op = rhiLoadOp::load;
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

	const rhiDescriptorBufferInfo ibl{
		.buffer = ibl_param_cbuffer[image_index.value()].get(),
		.offset = 0,
		.range = sizeof(lightingPass::light)
	};
	const rhiWriteDescriptor ibl_cb{
		.set = descriptor_sets[image_index.value()][0] ,
		.binding = 2,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::uniform_buffer,
		.buffer = { ibl }
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
		.binding = 3,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampled_image,
		.image = { mk_img(texture_context.gbuf_a)}
	};
	const rhiWriteDescriptor gbuf_b_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 4,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampled_image,
		.image = { mk_img(texture_context.gbuf_b)}
	};
	const rhiWriteDescriptor gbuf_c_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 5,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampled_image,
		.image = { mk_img(texture_context.gbuf_c)}
	};
	const rhiWriteDescriptor depth_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 6,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampled_image,
		.image = { rhiDescriptorImageInfo{
				.sampler = nullptr,
				.texture = texture_context.depth,
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
		.binding = 7,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampler,
		.image = { sampler_image_info }
	};

	const rhiWriteDescriptor shadow_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 8,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampled_image,
		.image = { rhiDescriptorImageInfo{
				.sampler = nullptr,
				.texture = texture_context.shadows,
				.mip = 0,
				.base_layer = 0,
				.layer_count = texture_context.shadows->desc.layers,
				.layout = rhiImageLayout::shader_readonly
			}
		}
	};

	const rhiDescriptorImageInfo shadow_sampler_image_info{
		.sampler = rs->samplers.point_clamp.get()
	};
	const rhiWriteDescriptor shadow_sampler_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 9,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampler,
		.image = { shadow_sampler_image_info }
	};

	const rhiWriteDescriptor ibl_irradiance_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 10,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampled_image,
		.image = { 
			rhiDescriptorImageInfo{
				.texture_cubemap = texture_context.ibl_irradiance,
				.layer_count = texture_context.ibl_irradiance->desc.layers,
				.cubemap_viewtype = rhiCubemapViewType::cube,
			}
		}
	};

	const rhiWriteDescriptor ibl_specular_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 11,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampled_image,
		.image = {
			rhiDescriptorImageInfo{
				.texture_cubemap = texture_context.ibl_specular,
				.layer_count = texture_context.ibl_specular->desc.layers,
				.cubemap_viewtype = rhiCubemapViewType::cube,
			}
		}
	};

	const rhiWriteDescriptor ibl_brdf_lut_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 12,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampled_image,
		.image = {
			rhiDescriptorImageInfo{
				.texture = texture_context.ibl_brdf_lut,
			}
		}
	};

	rs->context->update_descriptors({ 
		cam_cb,
		light_cb,
		ibl_cb,
		gbuf_a_write_desc,
		gbuf_b_write_desc,
		gbuf_c_write_desc,
		depth_write_desc,
		sampler_write_desc,
		shadow_write_desc,
		shadow_sampler_write_desc,
		ibl_irradiance_write_desc,
		ibl_specular_write_desc,
		ibl_brdf_lut_write_desc
		});
}

void lightingPass::begin_barrier(rhiCommandList* cmd)
{
	cmd->image_barrier(texture_context.scene_color, rhiImageLayout::color_attachment, rhiImageLayout::color_attachment);
}

void lightingPass::end_barrier(rhiCommandList* cmd)
{
	cmd->image_barrier(texture_context.scene_color, rhiImageLayout::color_attachment, rhiImageLayout::shader_readonly);
}

void lightingPass::draw(rhiCommandList* cmd)
{
	cmd->draw_fullscreen();
}