#include "oitResolvePass.h"
#include "renderShared.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiFrameContext.h"

void oitResolvePass::build_layouts(renderShared* rs)
{
	set_textures = rs->context->create_descriptor_set_layout({
			rhiDescriptorSetLayoutBinding{
				.binding = 0,
				.type = rhiDescriptorType::sampled_image,
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 1,
				.type = rhiDescriptorType::sampled_image,
			},
			rhiDescriptorSetLayoutBinding{
				.binding = 2,
				.type = rhiDescriptorType::sampler,
			},
		}, 0);
	create_pipeline_layout(rs, { set_textures });
	create_descriptor_sets(rs, { set_textures });
}

void oitResolvePass::build_attachments(rhiDeviceContext* context)
{
	auto ctx = static_cast<oitInitContext*>(init_context.get());
	ASSERT(ctx);
	auto sc = ctx->scene_color.lock();
	ASSERT(sc);

	render_info.renderpass_name = "oit resolve";
	render_info.samples = rhiSampleCount::x1;
	render_info.color_formats = { rhiFormat::RGBA8_UNORM };
	render_info.depth_format = std::nullopt;

	render_info.color_attachments.resize(1);
	render_info.color_attachments[0].view = rhiRenderTargetView{
		.texture = sc.get(),
		.mip = 0,
		.layout = rhiImageLayout::color_attachment
	};
	render_info.color_attachments[0].load_op = rhiLoadOp::load;
	render_info.color_attachments[0].store_op = rhiStoreOp::store;
	render_info.color_attachments[0].clear = { {0,0,0,1},1.0f,0 };
}

void oitResolvePass::build_pipeline(renderShared* rs)
{
	auto vs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\fullscreen.vs.spv");
	auto fs = shaderio::load_shader_binary("E:\\Sponza\\build\\shaders\\oit.ps.spv");
	const rhiGraphicsPipelineDesc pipeline_desc{
		.vs = vs,
		.fs = fs,
		.color_formats = { rhiFormat::RGBA8_UNORM },
		.depth_format = std::nullopt,
		.blend_states = { rhiBlendState{
			.src_color = rhiBlendFactor::one,
			.dst_color = rhiBlendFactor::one_minus_src_alpha,
			.src_alpha = rhiBlendFactor::one,
			.dst_alpha = rhiBlendFactor::one_minus_src_alpha,
			.color_blend = rhiBlendOp::add,
			.alpha_blend = rhiBlendOp::add,
			.color_write_mask = rhiColorComponentBit::r | rhiColorComponentBit::g | rhiColorComponentBit::b | rhiColorComponentBit::a
		}},
		.samples = rhiSampleCount::x1,
		.depth_test = false,
		.depth_write = false
	};

	pipeline = rs->context->create_graphics_pipeline(pipeline_desc, pipeline_layout);

	shaderio::free_shader_binary(vs);
	shaderio::free_shader_binary(fs);
}

void oitResolvePass::update(drawUpdateContext* update_context)
{
	auto ctx = static_cast<oitUpdateContext*>(update_context);
	ASSERT(ctx);
	
	const rhiWriteDescriptor accum_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 0,
		.count = 1,
		.type = rhiDescriptorType::sampled_image,
		.image = { 
			rhiDescriptorImageInfo{
				.texture = ctx->accum
			} 
		}
	};
	const rhiWriteDescriptor reveal_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 1,
		.count = 1,
		.type = rhiDescriptorType::sampled_image,
		.image = {
			rhiDescriptorImageInfo{
				.texture = ctx->reveal
			}
		}
	};
	const rhiWriteDescriptor sampler_write_desc{
		.set = descriptor_sets[image_index.value()][0],
		.binding = 2,
		.array_index = 0,
		.count = 1,
		.type = rhiDescriptorType::sampler,
		.image = {
			rhiDescriptorImageInfo{
				.sampler = init_context->rs->samplers.linear_clamp.get()
			}
		}
	};
	init_context->rs->context->update_descriptors({ accum_write_desc, reveal_write_desc, sampler_write_desc });
}

void oitResolvePass::draw(rhiCommandList* cmd)
{
	cmd->draw_fullscreen();
}

void oitResolvePass::begin_barrier(rhiCommandList* cmd)
{
	auto context = static_cast<oitInitContext*>(init_context.get());
	ASSERT(context);
	auto scene_color = context->scene_color.lock();
	ASSERT(scene_color);

	cmd->image_barrier(scene_color.get(), rhiImageBarrierDescription{
			.src_stage = rhiPipelineStage::compute_shader | rhiPipelineStage::fragment_shader,
			.dst_stage = rhiPipelineStage::color_attachment_output,
			.src_access = rhiAccessFlags::shader_sampled_read | rhiAccessFlags::shader_read,
			.dst_access = rhiAccessFlags::color_attachment_read | rhiAccessFlags::color_attachment_write,
			.old_layout = rhiImageLayout::shader_readonly,
			.new_layout = rhiImageLayout::color_attachment
		});
}

void oitResolvePass::end_barrier(rhiCommandList* cmd)
{
	auto context = static_cast<oitInitContext*>(init_context.get());
	ASSERT(context);
	auto scene_color = context->scene_color.lock();
	ASSERT(scene_color);

	cmd->image_barrier(scene_color.get(), rhiImageBarrierDescription{
			.src_stage = rhiPipelineStage::color_attachment_output,
			.dst_stage = rhiPipelineStage::compute_shader | rhiPipelineStage::fragment_shader,
			.src_access = rhiAccessFlags::color_attachment_read | rhiAccessFlags::color_attachment_write,
			.dst_access = rhiAccessFlags::shader_sampled_read | rhiAccessFlags::shader_read,
			.old_layout = rhiImageLayout::color_attachment,
			.new_layout = rhiImageLayout::shader_readonly
		});
}