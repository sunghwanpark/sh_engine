#include "translucentPass.h"
#include "renderShared.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiFrameContext.h"
#include "rhi/rhiCommandList.h"
#include "rhi/rhiRenderpass.h"
#include "mesh/glTFMesh.h"

void translucentPass::initialize(const drawInitContext& context)
{
	accumulate_color_alpha = context.rs->context->create_texture({
		.width = context.w,
		.height = context.h,
		.layers = context.layers,
		.mips = 1,
		.format = rhiFormat::RGBA16F,
		});
	revealage = context.rs->context->create_texture({
		.width = context.w,
		.height = context.h,
		.layers = context.layers,
		.mips = 1,
		.format = rhiFormat::RG16_SFLOAT
		});
	draw_type = drawType::translucent;
	
	drawPass::initialize(context);

	light_ring_buffer.resize(context.rs->frame_context->get_frame_size());
	for (u32 i = 0; i < context.rs->frame_context->get_frame_size(); ++i)
	{
		context.rs->create_or_resize_buffer(light_ring_buffer[i], sizeof(translucentPass::lightCB), rhiBufferUsage::uniform | rhiBufferUsage::transfer_dst, rhiMem::auto_device, 0);
	}
}

void translucentPass::begin(rhiCommandList* cmd)
{
	drawPass::begin(cmd);

	auto ptr = static_cast<translucentInitContext*>(init_context.get());
	ASSERT(ptr);

	auto table_ptr = ptr->bindless_table.lock();
	ASSERT(table_ptr);
	table_ptr->bind_once(cmd, pipeline_layout, 3);
}

void translucentPass::draw(rhiCommandList* cmd)
{
	auto buffer_ptr = indirect_buffer->at(static_cast<u8>(draw_type));
	auto group_record = group_records->at(static_cast<u8>(draw_type));
	cmd->bind_pipeline(pipeline.get());

	constexpr u32 stride = sizeof(rhiDrawIndexedIndirect);
	for (const auto& g : group_record)
	{
		push_constants(cmd, g);
		cmd->bind_vertex_buffer(const_cast<rhiBuffer*>(g.vbo), 0, 0);
		cmd->bind_index_buffer(const_cast<rhiBuffer*>(g.ibo), 0);
		const u32 byte_offset = g.first_cmd * stride;
		cmd->draw_indexed_indirect(buffer_ptr.get(), byte_offset, g.cmd_count, stride);
	}
}

void translucentPass::begin_barrier(rhiCommandList* cmd)
{
	if (is_first_frame)
	{
		rhiImageBarrierDescription common_desc{
			.src_stage = rhiPipelineStage::none,
			.dst_stage = rhiPipelineStage::color_attachment_output,
			.src_access = rhiAccessFlags::none,
			.dst_access = rhiAccessFlags::color_attachment_write,
			.old_layout = rhiImageLayout::undefined,
			.new_layout = rhiImageLayout::color_attachment
		};
		cmd->image_barrier(accumulate_color_alpha.get(), common_desc);
		cmd->image_barrier(revealage.get(), common_desc);
		is_first_frame = false;
	}
	else
	{
		rhiImageBarrierDescription common_desc{
			.src_stage = rhiPipelineStage::fragment_shader,
			.dst_stage = rhiPipelineStage::color_attachment_output,
			.src_access = rhiAccessFlags::shader_read,
			.dst_access = rhiAccessFlags::color_attachment_write,
			.old_layout = rhiImageLayout::shader_readonly,
			.new_layout = rhiImageLayout::color_attachment
		};
		cmd->image_barrier(accumulate_color_alpha.get(), common_desc);
		cmd->image_barrier(revealage.get(), common_desc);
	}

	auto ptr = static_cast<translucentInitContext*>(init_context.get());
	ASSERT(ptr);

	cmd->image_barrier(ptr->depth, rhiImageBarrierDescription{
		.src_stage = rhiPipelineStage::compute_shader | rhiPipelineStage::fragment_shader,
		.dst_stage = rhiPipelineStage::early_fragment_test | rhiPipelineStage::late_fragment_test,
		.src_access = rhiAccessFlags::shader_read | rhiAccessFlags::shader_sampled_read,
		.dst_access = rhiAccessFlags::depthstencil_attachment_read | rhiAccessFlags::depthstencil_attachment_write,
		.old_layout = rhiImageLayout::shader_readonly,
		.new_layout = rhiImageLayout::depth_stencil_attachment,
		.layer_count = ptr->depth->desc.layers
		});
}

void translucentPass::end_barrier(rhiCommandList* cmd)
{
	rhiImageBarrierDescription common_desc{
		.src_stage = rhiPipelineStage::color_attachment_output,
		.dst_stage = rhiPipelineStage::fragment_shader,
		.src_access = rhiAccessFlags::color_attachment_write,
		.dst_access = rhiAccessFlags::shader_read,
		.old_layout = rhiImageLayout::color_attachment,
		.new_layout = rhiImageLayout::shader_readonly
	};
	cmd->image_barrier(accumulate_color_alpha.get(), common_desc);
	cmd->image_barrier(revealage.get(), common_desc);

	auto ptr = static_cast<translucentInitContext*>(init_context.get());
	ASSERT(ptr);
	cmd->image_barrier(ptr->depth, rhiImageBarrierDescription{
		.src_stage = rhiPipelineStage::early_fragment_test | rhiPipelineStage::late_fragment_test,
		.dst_stage = rhiPipelineStage::compute_shader | rhiPipelineStage::fragment_shader,
		.src_access = rhiAccessFlags::depthstencil_attachment_read | rhiAccessFlags::depthstencil_attachment_write,
		.dst_access = rhiAccessFlags::shader_read | rhiAccessFlags::shader_sampled_read,
		.old_layout = rhiImageLayout::depth_stencil_attachment,
		.new_layout = rhiImageLayout::shader_readonly,
		.layer_count = ptr->depth->desc.layers
		});
}

void translucentPass::build_layouts(renderShared* rs)
{
	set_globals = rs->context->create_descriptor_set_layout({
			rhiDescriptorSetLayoutBinding{
				.binding = 0,
				.type = rhiDescriptorType::uniform_buffer,
				.count = 1,
				.stage = rhiShaderStage::vertex | rhiShaderStage::fragment
			}
		}, 0);
	set_instances = rs->context->create_descriptor_set_layout({
			rhiDescriptorSetLayoutBinding{
				.binding = 0,
				.type = rhiDescriptorType::storage_buffer,
				.count = 1,
				.stage = rhiShaderStage::vertex
			}
		}, 1);
	set_light = rs->context->create_descriptor_set_layout({
			rhiDescriptorSetLayoutBinding{
				.binding = 0,
				.type = rhiDescriptorType::uniform_buffer,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 1,
				.type = rhiDescriptorType::sampled_image,
				.count = 1,
				.stage = rhiShaderStage::fragment
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 2,
				.type = rhiDescriptorType::sampler,
				.count = 1,
				.stage = rhiShaderStage::fragment
			}
		}, 2);

	auto ptr = static_cast<translucentInitContext*>(init_context.get());
	ASSERT(ptr);

	auto table_ptr = ptr->bindless_table.lock();
	ASSERT(table_ptr);

	set_material = table_ptr->get_set_layout();
	create_pipeline_layout(rs, { set_globals, set_instances, set_light, set_material }, { { rhiShaderStage::fragment, sizeof(materialPC) } });
	create_descriptor_sets(rs, { set_globals, set_instances, set_light });
}

void translucentPass::build_attachments(rhiDeviceContext* context)
{
	auto ptr = static_cast<translucentInitContext*>(init_context.get());
	ASSERT(ptr);

	auto depth = ptr->depth;
	ASSERT(depth);

	render_info.renderpass_name = "translucent";
	render_info.samples = rhiSampleCount::x1;
	render_info.color_formats = { rhiFormat::RGBA16F, rhiFormat::RG16_SFLOAT };
	render_info.depth_format = rhiFormat::D32S8;

	render_info.color_attachments.resize(2);
	
	rhiRenderTargetView rt1{
		.texture = accumulate_color_alpha.get(),
		.layout = rhiImageLayout::color_attachment
	};
	render_info.color_attachments[0].view = rt1;
	render_info.color_attachments[0].clear = { {0,0,0,0},1.0f, 0 };
	rhiRenderTargetView rt2{
		.texture = revealage.get(),
		.layout = rhiImageLayout::color_attachment
	};
	render_info.color_attachments[1].view = rt2;
	render_info.color_attachments[1].clear = { {1,0,0,0},1.0f,0 };
	for (u32 index = 0; index < 2; ++index)
	{
		render_info.color_attachments[index].load_op = rhiLoadOp::clear;
		render_info.color_attachments[index].store_op = rhiStoreOp::store;
	}

	rhiRenderTargetView d{
		.texture = depth,
		.layout = rhiImageLayout::depth_readonly
	};
	rhiRenderingAttachment depth_attachment;
	depth_attachment.view = d;
	depth_attachment.load_op = rhiLoadOp::load;
	depth_attachment.store_op = rhiStoreOp::store;
	render_info.depth_attachment = depth_attachment;
}

void translucentPass::build_pipeline(renderShared* rs)
{
	auto ptr = static_cast<translucentInitContext*>(init_context.get());
	ASSERT(ptr);

	auto depth = ptr->depth;
	ASSERT(depth);

	auto vs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\translucent.vs.spv");
	auto fs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\translucent.ps.spv");
	rhiGraphicsPipelineDesc desc{
		.vs = vs,
		.fs = fs,
		.color_formats = {accumulate_color_alpha->desc.format, revealage->desc.format},
		.depth_format = depth->desc.format,
		.blend_states = {
			// accumulate
			rhiBlendState{
				.src_color = rhiBlendFactor::one,
				.dst_color = rhiBlendFactor::one,
				.src_alpha = rhiBlendFactor::one,
				.dst_alpha = rhiBlendFactor::one,
				.color_blend = rhiBlendOp::add,
				.alpha_blend = rhiBlendOp::add,
				.color_write_mask = rhiColorComponentBit::r | rhiColorComponentBit::g | rhiColorComponentBit::b | rhiColorComponentBit::a
			},
			//reveal
			rhiBlendState{
				.src_color = rhiBlendFactor::zero,
				.dst_color = rhiBlendFactor::one_minus_src_alpha,
				.src_alpha = rhiBlendFactor::zero,
				.dst_alpha = rhiBlendFactor::one,
				.color_blend = rhiBlendOp::add,
				.alpha_blend = rhiBlendOp::add,
				.color_write_mask = rhiColorComponentBit::r
			}
		},
		.samples = rhiSampleCount::x1,
		.depth_test = true,
		.depth_write = false,
		//.use_dynamic_cullmode = true,
		.vertex_layout = rhiVertexAttribute{
			.binding = 0,
			.stride = sizeof(glTFVertex),
			.vertex_attr_desc = {
				rhiVertexAttributeDesc{ 0, 0, rhiFormat::RGB32_SFLOAT, offsetof(glTFVertex, position) },
				rhiVertexAttributeDesc{ 1, 0, rhiFormat::RGB32_SFLOAT, offsetof(glTFVertex, normal) },
				rhiVertexAttributeDesc{ 2, 0, rhiFormat::RG32_SFLOAT, offsetof(glTFVertex, uv) },
				rhiVertexAttributeDesc{ 3, 0, rhiFormat::RGBA32_SFLOAT, offsetof(glTFVertex, tangent) },
		}}
	};
	pipeline = rs->context->create_graphics_pipeline(desc, pipeline_layout);

	shaderio::free_shader_binary(vs);
	shaderio::free_shader_binary(fs);
}

void translucentPass::update(drawUpdateContext* update_context)
{
	auto update_ptr = static_cast<translucentUpdateContext*>(update_context);
	ASSERT(update_ptr);

	update_buffer(update_ptr);

	const rhiDescriptorBufferInfo global_buffer_info{
		.buffer = update_ptr->global_buffer,
		.offset = 0,
		.range = sizeof(globalsCB)
	};
	const rhiWriteDescriptor global_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 0,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::uniform_buffer,
		.buffer = { global_buffer_info }
	};
	const rhiDescriptorBufferInfo light_buffer_info{
		.buffer = light_ring_buffer[image_index.value()].get(),
		.offset = 0,
		.range = sizeof(translucentPass::lightCB)
	};
	const rhiWriteDescriptor light_write_desc{
		.set = descriptor_sets[image_index.value()][2],
		.binding = 0,
		.count = 1,
		.type = rhiDescriptorType::uniform_buffer,
		.buffer = { light_buffer_info }
	};
	const rhiWriteDescriptor shadow_write_desc{
		.set = descriptor_sets[image_index.value()][2],
		.binding = 1,
		.count = 1,
		.type = rhiDescriptorType::sampled_image,
		.image = { rhiDescriptorImageInfo{
				.texture = update_ptr->shadow_depth,
				.mip =0,
				.base_layer = 0,
				.layer_count = update_ptr->shadow_depth->desc.layers,
				.layout = rhiImageLayout::shader_readonly
			} 
		}
	};
	const rhiWriteDescriptor shadow_sampler_write_desc{
		.set = descriptor_sets[image_index.value()][2],
		.binding = 2,
		.count = 1,
		.type = rhiDescriptorType::sampler,
		.image = { rhiDescriptorImageInfo{.sampler = init_context->rs->samplers.linear_clamp.get() } }
	};

	init_context->rs->context->update_descriptors({ global_write_desc, light_write_desc, shadow_write_desc, shadow_sampler_write_desc });

	update_instances(init_context->rs, 1);
}

void translucentPass::update_buffer(translucentUpdateContext* update_context)
{
	ASSERT(update_context->light_viewproj.size() >= 4);
	translucentPass::lightCB cb{
		.light_dir = update_context->light_dir,
		.cascade_splits = vec4(update_context->cascade_splits[0], update_context->cascade_splits[1], update_context->cascade_splits[2], update_context->cascade_splits[3]),
		.shadow_mapsize = vec2(update_context->shadow_mapsize, update_context->shadow_mapsize)
	};
	for (u32 index = 0; index < update_context->light_viewproj.size(); ++index)
	{
		cb.light_viewproj[index] = update_context->light_viewproj[index];
	}
	auto device_context = init_context->rs->context;
	init_context->rs->buffer_barrier(light_ring_buffer[image_index.value()].get(), rhiBufferBarrierDescription{
				.src_stage = rhiPipelineStage::fragment_shader,
				.dst_stage = rhiPipelineStage::copy,
				.src_access = rhiAccessFlags::uniform_read,
				.dst_access = rhiAccessFlags::transfer_write,
				.size = sizeof(translucentPass::lightCB),
				.src_queue = device_context->get_queue_family_index(rhiQueueType::transfer),
				.dst_queue = device_context->get_queue_family_index(rhiQueueType::graphics)
		});
	init_context->rs->upload_to_device(light_ring_buffer[image_index.value()].get(), &cb, sizeof(translucentPass::lightCB));
	init_context->rs->buffer_barrier(light_ring_buffer[image_index.value()].get(), rhiBufferBarrierDescription{
				.src_stage = rhiPipelineStage::copy,
				.dst_stage = rhiPipelineStage::vertex_shader | rhiPipelineStage::fragment_shader,
				.src_access = rhiAccessFlags::transfer_write,
				.dst_access = rhiAccessFlags::uniform_read,
				.size = sizeof(translucentPass::lightCB),
				.src_queue = device_context->get_queue_family_index(rhiQueueType::transfer),
				.dst_queue = device_context->get_queue_family_index(rhiQueueType::graphics)
		});
}

void translucentPass::push_constants(rhiCommandList* cmd, const groupRecord& g)
{
	materialPC pc = g;
	cmd->push_constants(pipeline_layout, rhiShaderStage::fragment, 0, sizeof(pc), &pc);
}