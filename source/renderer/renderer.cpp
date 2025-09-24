#include "renderer.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiRenderResource.h"
#include "rhi/rhiBuffer.h"
#include "rhi/rhiCommandList.h"
#include "rhi/rhiFrameContext.h"
#include "rhi/rhiSwapChain.h"
#include "rhi/rhiSubmitInfo.h"
#include "rhi/rhiGraphicsQueue.h"
#include "rhi/rhiSynchroize.h"
#include "rhi/rhiQueue.h"
#include "scene/scene.h"
#include "scene/camera.h"
#include "scene/light/directionalLightActor.h"
#include "scene/actor/meshActor.h"
#include "scene/actor/component/meshComponent.h"
#include "scene/actor/component/transformComponent.h"
#include "mesh/meshModelManager.h"
#include "mesh/glTFMesh.h"

renderer::renderer()
{
}

renderer::~renderer()
{
    bindless_table.reset();
    instance_buffer.reset();
    indirect_buffer.reset();
    global_ringbuffer.clear();
    texture_cache->clear();
}

void renderer::initialize(scene* s, rhiDeviceContext* device_context, rhiFrameContext* frame_context)
{
    const u32 width = frame_context->swapchain->width();
    const u32 height = frame_context->swapchain->height();
    framebuffer_size.x = width;
    framebuffer_size.y = height;

    texture_cache = std::make_unique<textureCache>(device_context);
    bindless_table = device_context->create_bindless_table(rhiBindlessDesc(), 2);

    render_shared.Initialize(device_context, frame_context);
    {
        gbufferInitContext ctx{};
        ctx.rs = &render_shared;
        ctx.w = width;
        ctx.h = height;
        ctx.bindless_table = bindless_table;
        gbuffer_pass.initialize(ctx);
    }

    {
        skyInitContext ctx{};
        ctx.rs = &render_shared;
        ctx.w = width;
        ctx.h = height;
        ctx.scene_color = render_shared.scene_color;
        sky_pass.initialize(ctx);
    }
    
    {
        shadowInitContext ctx{};
        ctx.rs = &render_shared;
        ctx.w = width;
        ctx.h = height;
        ctx.s = s;
        ctx.framebuffer_size = framebuffer_size;
        shadow_pass.initialize(ctx);
    }
    
    {
        drawInitContext ctx{};
        ctx.rs = &render_shared;
        ctx.w = width;
        ctx.h = height;
        lighting_pass.initialize(ctx);
    }

    {
        compositeInitContext ctx{};
        ctx.rs = &render_shared;
        ctx.w = width;
        ctx.h = height;
        ctx.swapchain_views = frame_context->swapchain->views();

        composite_pass.initialize(ctx);
    }

    create_ringbuffer(render_shared.get_frame_size());
    
}

void renderer::pre_render(scene* s)
{
    const u32 width = render_shared.frame_context->swapchain->width();
    const u32 height = render_shared.frame_context->swapchain->height();
    if (framebuffer_size.x != width || framebuffer_size.y != height)
    {
        gbuffer_pass.shutdown();
        shadow_pass.shutdown();
        sky_pass.shutdown();
        lighting_pass.shutdown();
        composite_pass.shutdown();
        initialize(s, render_shared.context, render_shared.frame_context);
    }
}

void renderer::render(scene* s)
{
    auto device_context = render_shared.context;
    auto frame_context = render_shared.frame_context;

    frame_context->wait(device_context);
    frame_context->reset(device_context);

    u32 img_index = 0;
    frame_context->acquire_next_image(&img_index);
    if (img_index == 0)
        render_shared.clear_staging_buffer();

    notify_nextimage_index_to_drawpass(img_index);

    frame_context->command_begin();

    if(!initialized)
    {
        sky_pass.precompile_dispatch();
        prepare(s);
        build(s, device_context);
        render_shared.image_barrier(frame_context->swapchain->views()[img_index].texture, rhiImageBarrierDescription{
            .src_stage = rhiPipelineStage::color_attachment_output,
            .dst_stage = rhiPipelineStage::none,
            .src_access = rhiAccessFlags::color_attachment_write,
            .dst_access = rhiAccessFlags::none,
            .old_layout = rhiImageLayout::undefined,
            .new_layout = rhiImageLayout::present,
            });
    }
    else
    {
        // build global view_proj
        {
            globalsCB cb;
            cb.view = s->get_camera()->view();
            cb.proj = s->get_camera()->proj(framebuffer_size);
            cb.inv_view_proj = glm::inverse(cb.proj * cb.view);
            cb.cam_pos = vec4(s->get_camera()->get_position(), 0.f);

            render_shared.buffer_barrier(global_ringbuffer[img_index].get(), rhiBufferBarrierDescription{
                .src_stage = rhiPipelineStage::vertex_shader | rhiPipelineStage::fragment_shader,
                .dst_stage = rhiPipelineStage::copy,
                .src_access = rhiAccessFlags::uniform_read,
                .dst_access = rhiAccessFlags::transfer_write,
                .size = sizeof(globalsCB),
                .src_queue = device_context->get_queue_family_index(rhiQueueType::transfer),
                .dst_queue = device_context->get_queue_family_index(rhiQueueType::graphics)
                });
            render_shared.upload_to_device(global_ringbuffer[img_index].get(), &cb, sizeof(globalsCB));
            render_shared.buffer_barrier(global_ringbuffer[img_index].get(), rhiBufferBarrierDescription{
                .src_stage = rhiPipelineStage::copy,
                .dst_stage = rhiPipelineStage::vertex_shader | rhiPipelineStage::fragment_shader,
                .src_access = rhiAccessFlags::transfer_write,
                .dst_access = rhiAccessFlags::uniform_read,
                .size = sizeof(globalsCB),
                .src_queue = device_context->get_queue_family_index(rhiQueueType::transfer),
                .dst_queue = device_context->get_queue_family_index(rhiQueueType::graphics)
                });
        }

        // shadow pass
        {
            shadow_pass.update(&render_shared, s, framebuffer_size);
            shadow_pass.render(&render_shared);
        }

        // gbuffer pass
        {
            gbuffer_pass.update(&render_shared, global_ringbuffer[img_index].get(), instance_buffer.get());
            gbuffer_pass.render(&render_shared);
        }

        // sky pass
        {
            std::unique_ptr<skyUpdateContext> context = std::make_unique<skyUpdateContext>(global_ringbuffer[img_index].get());
            sky_pass.update(context.get());
            sky_pass.render(&render_shared);
        }

        // lighting pass
        {
            lightingPass::textureContext ctx{
                .scene_color = render_shared.scene_color.get(),
                .gbuf_a = gbuffer_pass.get_gbuffer_a(),
                .gbuf_b = gbuffer_pass.get_gbuffer_b(),
                .gbuf_c = gbuffer_pass.get_gbuffer_c(),
                .depth = gbuffer_pass.get_depth(),
                .shadows = shadow_pass.get_shadow_texture(),
                .ibl_irradiance = sky_pass.get_irradiance_map(),
                .ibl_specular = sky_pass.get_specular_map(),
                .ibl_brdf_lut = sky_pass.get_brdf_lut_map()
            };
            lighting_pass.link_textures(ctx);
            lighting_pass.update(
                &render_shared,
                s->get_camera(),
                s->get_directional_light()->get_direction(),
                shadow_pass.get_light_viewproj(),
                shadow_pass.get_cascade_splits(),
                shadow_pass.get_width(),
                sky_pass.get_cubemap_mip_count());
            lighting_pass.render(&render_shared);
        }

        // translucent pass
        {

        }

        // composite
        {
            composite_pass.update(&render_shared, render_shared.scene_color.get());
            composite_pass.render(&render_shared);
        }
    }
    frame_context->command_end();
    // queue submit
    frame_context->submit(device_context, img_index);

    // present
    frame_context->present(img_index);
    initialized = true;
}

void renderer::post_render()
{

}

std::shared_ptr<rhiRenderResource> renderer::get_or_create_resource(const std::shared_ptr<glTFMesh> raw_mesh)
{
    if(cache.contains(raw_mesh->hash()))
        return cache[raw_mesh->hash()];

    auto rhi_resource = std::make_shared<rhiRenderResource>(raw_mesh);
    assert(rhi_resource);
    rhi_resource->upload(&render_shared, texture_cache.get());
    rhi_resource->set_hash(raw_mesh->hash());
    cache.emplace(raw_mesh->hash(), std::move(rhi_resource));
    return cache[raw_mesh->hash()];
}

void renderer::prepare(scene* s)
{
    for (auto& a : s->get_actors()) 
    {
        meshActor* mesh_actor = static_cast<meshActor*>(a.get());
        if (mesh_actor == nullptr)
            continue;

        auto raw_mesh = mesh_actor->mesh.get();
        if (raw_mesh == nullptr)
            continue;

        auto mgr = s->get_mesh_manager();
        auto mesh_obj = mgr->get_asset(mesh_actor->mesh_path);
        if (mesh_obj.expired())
            continue;

        get_or_create_resource(mesh_obj.lock());
    }
}

void renderer::build(scene* s, rhiDeviceContext* context)
{
    instances.clear();
    indirect_args.clear();
    groups.clear();
    std::unordered_map<drawGroupKey, std::vector<rhiDrawIndexedIndirect>, drawGroupKeyHash> map;
    std::unordered_map<u32, bool> double_sided;

    for (auto& a : s->get_actors())
    {
        meshActor* mesh_actor = static_cast<meshActor*>(a.get());
        if (mesh_actor == nullptr)
            continue;

        if (!cache.contains(mesh_actor->get_mesh_hash()))
            continue;

        const auto actor_mat = mesh_actor->transform->matrix();
        
        auto rhi_resource = cache[mesh_actor->get_mesh_hash()];
        auto vbo = rhi_resource->get_vbo();
        auto ibo = rhi_resource->get_ibo();

        // create submesh indirect command
        for (const auto& sub_mesh : rhi_resource->get_submeshes())
        {
            const auto& mat = rhi_resource->get_material(sub_mesh.material_slot);
            rhiTexture* base = mat.base_color.get();
            rhiTexture* norm = mat.norm_color.get();
            rhiTexture* mr = mat.m_r_color.get();
            if (!base || !norm || !mr)
                continue;
            rhiSampler* base_sam = mat.base_sampler.get();
            rhiSampler* norm_sam = mat.norm_sampler.get();
            rhiSampler* mr_sam = mat.m_r_sampler.get();
            if (!base_sam || !norm_sam || !mr_sam)
                continue;

            const auto submesh_mat = actor_mat * sub_mesh.model;
            instanceData inst{
                .model = submesh_mat,
                .normal_mat = glm::transpose(glm::inverse(glm::mat3(submesh_mat)))
            };
            const u32 first_instance = static_cast<u32>(instances.size());
            instances.push_back(inst);

            auto base_tex_bindless_handle = bindless_table->register_sampled_image(base);
            auto norm_tex_bindless_handle = bindless_table->register_sampled_image(norm);
            auto m_r_tex_bindless_handle = bindless_table->register_sampled_image(mr);
            auto base_sam_bindless_handle = bindless_table->register_sampler(base_sam);
            auto norm_sam_bindless_handle = bindless_table->register_sampler(norm_sam);
            auto m_r_sam_bindless_handle = bindless_table->register_sampler(mr_sam);
            const drawGroupKey key
            {
                .vbo = vbo,
                .ibo = ibo,
                .base_color_index = base_tex_bindless_handle.index,
                .norm_color_index = norm_tex_bindless_handle.index,
                .m_r_color_index = m_r_tex_bindless_handle.index,
                .base_sam_index = base_sam_bindless_handle.index,
                .norm_sam_index = norm_sam_bindless_handle.index,
                .m_r_sam_index = m_r_sam_bindless_handle.index,
                .alpha_cutoff = mat.alpha_cutoff,
                .metalic_factor = mat.metalic_factor,
                .roughness_factor = mat.roughness_factor,
                .is_double_sided = mat.is_double_sided
            };

            const rhiDrawIndexedIndirect indirect_cmd
            {
                .index_count = sub_mesh.index_count,
                .instance_count = 1,
                .first_index = sub_mesh.first_index,
                .vertex_offset = 0,
                .first_instance = first_instance
            };
            map[key].push_back(indirect_cmd);
        }
    }

    // sort indirect_args
    u32 running = 0;
    groups.reserve(map.size());
    for (auto& [key, cmds] : map)
    {
        const groupRecord group_record
        {
            .vbo = key.vbo,
            .ibo = key.ibo,
            .first_cmd = running,
            .cmd_count = static_cast<u32>(cmds.size()),
            .base_color_index = key.base_color_index,
            .norm_color_index = key.norm_color_index,
            .m_r_color_index = key.m_r_color_index,
            .base_sam_index = key.base_sam_index,
            .norm_sam_index = key.norm_sam_index,
            .m_r_sam_index = key.m_r_sam_index,
            .alpha_cutoff = key.alpha_cutoff,
            .metalic_factor = key.metalic_factor,
            .roughness_factor = key.roughness_factor
        };

        groups.push_back(group_record);
        indirect_args.insert(indirect_args.end(), cmds.begin(), cmds.end());
        double_sided.emplace(running, key.is_double_sided);
        running += group_record.cmd_count;
    }

    // create gpu buffer / resize
    const u32 instance_bytes = static_cast<u32>(instances.size()) * sizeof(instanceData);
    const u32 indirect_bytes = static_cast<u32>(indirect_args.size()) * sizeof(rhiDrawIndexedIndirect);
    if (instance_bytes)
    {
        render_shared.create_or_resize_buffer(instance_buffer, instance_bytes, rhiBufferUsage::storage | rhiBufferUsage::transfer_dst, rhiMem::auto_device, sizeof(instanceData));
        render_shared.upload_to_device(instance_buffer.get(), instances.data(), instance_bytes);
        render_shared.buffer_barrier(instance_buffer.get(), { 
            .src_stage = rhiPipelineStage::copy, 
            .dst_stage = rhiPipelineStage::vertex_shader,
            .src_access = rhiAccessFlags::transfer_write, 
            .dst_access = rhiAccessFlags::shader_read | rhiAccessFlags::shader_storage_read,
            .offset = 0, 
            .size = instance_bytes, 
            .src_queue = render_shared.context->get_queue_family_index(rhiQueueType::graphics),
            .dst_queue = render_shared.context->get_queue_family_index(rhiQueueType::transfer) });
    }
    else
    {
        instance_buffer.reset();
    }

    if (indirect_bytes)
    {
        render_shared.create_or_resize_buffer(indirect_buffer, indirect_bytes, rhiBufferUsage::indirect | rhiBufferUsage::transfer_dst, rhiMem::auto_device, sizeof(rhiDrawIndexedIndirect));
        render_shared.upload_to_device(indirect_buffer.get(), indirect_args.data(), indirect_bytes);
        render_shared.buffer_barrier(indirect_buffer.get(), {
            .src_stage = rhiPipelineStage::copy,
            .dst_stage = rhiPipelineStage::draw_indirect,
            .src_access = rhiAccessFlags::transfer_write,
            .dst_access = rhiAccessFlags::indirect_command_read,
            .offset = 0,
            .size = indirect_bytes,
            .src_queue = render_shared.context->get_queue_family_index(rhiQueueType::graphics),
            .dst_queue = render_shared.context->get_queue_family_index(rhiQueueType::transfer) });
    }
    else
    {
        indirect_buffer.reset();
    }

    shadow_pass.update_elements(&groups, instance_buffer, indirect_buffer);
    gbuffer_pass.update_elements(&groups, instance_buffer, indirect_buffer);
    gbuffer_pass.update_double_sided_info(double_sided);
}

void renderer::create_ringbuffer(const u32 frame_size)
{
    global_ringbuffer.resize(frame_size);
    for (u32 index = 0; index < frame_size; ++index)
    {
        render_shared.create_or_resize_buffer(global_ringbuffer[index], sizeof(globalsCB), rhiBufferUsage::uniform | rhiBufferUsage::transfer_dst, rhiMem::auto_device, sizeof(globalsCB));
    }
}

void renderer::notify_nextimage_index_to_drawpass(const u32 image_index)
{
    shadow_pass.frame(image_index);
    gbuffer_pass.frame(image_index);
    sky_pass.frame(image_index);
    lighting_pass.frame(image_index);
    composite_pass.frame(image_index);
}