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
#include "rhi/rhiTextureBindlessTable.h"
#include "scene/scene.h"
#include "scene/camera.h"
#include "scene/light/directionalLightActor.h"
#include "scene/actor/meshActor.h"
#include "scene/actor/component/meshComponent.h"
#include "scene/actor/component/transformComponent.h"
#include "mesh/meshModelManager.h"
#include "mesh/glTFMesh.h"
#include "util/packing.h"

renderer::renderer()
{
}

renderer::~renderer()
{
    bindless_table.reset();
    for (u32 i = 0; i < draw_type_count; ++i)
    {
        instance_buffer[i].reset();
        indirect_buffer[i].reset();
    }
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
    bindless_table = device_context->create_bindless_table(rhiTextureBindlessDesc(), 2);

    render_shared.Initialize(device_context, frame_context);
    {
#if MESHLET
        gbufferPass_meshletInitContext ctx{};
        ctx.rs = &render_shared;
        ctx.w = width;
        ctx.h = height;
        ctx.bindless_table = bindless_table;
        gbuffer_pass.initialize(ctx);
#else
        gbufferInitContext ctx{};
        ctx.rs = &render_shared;
        ctx.w = width;
        ctx.h = height;
        ctx.bindless_table = bindless_table;
        gbuffer_pass.initialize(ctx);
#endif
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
        ctx.bindless_table = bindless_table;
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
        translucentInitContext ctx{};
        ctx.rs = &render_shared;
        ctx.w = width;
        ctx.h = height;
        ctx.bindless_table = bindless_table;
        ctx.depth = gbuffer_pass.get_depth();
#if DISABLE_OIT
        ctx.scene_color = render_shared.scene_color;
#endif
        translucent_pass.initialize(ctx);
    }

#if !DISABLE_OIT
    {
        oitInitContext ctx{};
        ctx.rs = &render_shared;
        ctx.w = width;
        ctx.h = height;
        ctx.scene_color = render_shared.scene_color;
        oit_pass.initialize(ctx);
    }
#endif

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
#if MESHLET
        build_meshlet(s);
#else
        build(s, device_context);
#endif
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
            cb.view_proj = cb.proj * cb.view;
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
#if !MESHLET
        {
            shadow_pass.update(&render_shared, s, framebuffer_size);
            shadow_pass.render(&render_shared);
        }
#endif

        // gbuffer pass
        {
#if MESHLET
            meshletDrawUpdateContext context{
                .global_buf = global_ringbuffer[img_index].get(),
                .meshlet_buf = &meshlet_ssbo
            };
            gbuffer_pass.update(&context);
            gbuffer_pass.render(&render_shared);
#else
            gbuffer_pass.update(&render_shared, global_ringbuffer[img_index].get());
            gbuffer_pass.render(&render_shared);
#endif
        }

        // sky pass
        {
            skyUpdateContext context{ global_ringbuffer[img_index].get() };
            sky_pass.update(&context);
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
#if !MESHLET
            translucentUpdateContext update_context{
                .global_buffer = global_ringbuffer[img_index].get(),
                .shadow_depth = shadow_pass.get_shadow_texture(),
                .light_viewproj = shadow_pass.get_light_viewproj(),
                .light_dir = vec4(s->get_directional_light()->get_direction(), 0.f),
                .cascade_splits = shadow_pass.get_cascade_splits(),
                .shadow_mapsize = static_cast<f32>(shadow_pass.get_width())
            };
            translucent_pass.update(&update_context);
            translucent_pass.render(&render_shared);
#endif
        }

        // oit pass
#if !DISABLE_OIT && !MESHLET
        {
            oitUpdateContext update_context{
                .accum = translucent_pass.get_accum(),
                .reveal = translucent_pass.get_reveal()
            };
            oit_pass.update(&update_context);
            oit_pass.render(&render_shared);
        }
#endif

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
    ASSERT(rhi_resource);
#if MESHLET
    rhi_resource->make_meshlet_resource(texture_cache.get());
#else
    rhi_resource->upload(&render_shared, texture_cache.get());
#endif
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

#if MESHLET
void renderer::build_meshlet(scene* s)
{
    meshlet::buildOut out;
    for (auto& a : s->get_actors())
    {
        auto* mesh_actor = static_cast<meshActor*>(a.get());
        if (!mesh_actor)
            continue;

        build_meshlet_global_vertices(mesh_actor, out);
    }
    build_meshlet_drawcommand(s);
    build_meshlet_ssbo(&out);
}

void renderer::build_meshlet_drawcommand(scene* s)
{
    std::unordered_map<uint32_t, bool> double_sided;
    meshletDrawParamArray meshlet_draw_params;
    drawMeshIndirectArray meshlet_indirect_args;

    std::array<std::unordered_map<drawGroupKey, std::vector<instanceData>, drawGroupKeyHash>, draw_type_count> buckets;
    std::array<std::unordered_map<drawGroupKey, rhiRenderResource::subMesh, drawGroupKeyHash>, draw_type_count> submesh_buckets;
    for (auto& a : s->get_actors())
    {
        auto* mesh_actor = static_cast<meshActor*>(a.get());
        if (!mesh_actor)
            continue;

        if (!cache.contains(mesh_actor->get_mesh_hash()))
            continue;

        const auto actor_mat = mesh_actor->transform->matrix();
        auto rhi_resource = cache[mesh_actor->get_mesh_hash()];

        auto vbo = rhi_resource->get_vbo();
        auto ibo = rhi_resource->get_ibo();

        for (auto& sub_mesh : rhi_resource->get_submeshes())
        {
            const auto& mat = rhi_resource->get_material(sub_mesh.material_slot);

            std::unordered_map<rhiTextureType, rhiBindlessHandle> bindless_handles;
            if (auto* base = mat.base_color.get())
                bindless_handles.emplace(rhiTextureType::base_color, bindless_table->register_sampled_image(base));
            if (auto* norm = mat.norm_color.get())
                bindless_handles.emplace(rhiTextureType::normal, bindless_table->register_sampled_image(norm));
            if (auto* mr = mat.m_r_color.get())
                bindless_handles.emplace(rhiTextureType::metalic_roughness, bindless_table->register_sampled_image(mr));
            if (auto* s0 = mat.base_sampler.get())
                bindless_handles.emplace(rhiTextureType::base_color_sampler, bindless_table->register_sampler(s0));
            if (auto* s1 = mat.norm_sampler.get())
                bindless_handles.emplace(rhiTextureType::normal_sampler, bindless_table->register_sampler(s1));
            if (auto* s2 = mat.m_r_sampler.get())
                bindless_handles.emplace(rhiTextureType::metalic_roughness_sampler, bindless_table->register_sampler(s2));

            const auto submesh_mat = actor_mat * sub_mesh.model;
            instanceData inst{
                .model = submesh_mat,
                .normal_mat = glm::transpose(glm::inverse(submesh_mat)),
            };

            const drawGroupKey key{
                .vbo = vbo,
                .ibo = ibo,
                .base_color_index = bindless_handles.count(rhiTextureType::base_color) ? bindless_handles[rhiTextureType::base_color].index : 0,
                .norm_color_index = bindless_handles.count(rhiTextureType::normal) ? bindless_handles[rhiTextureType::normal].index : 0,
                .m_r_color_index = bindless_handles.count(rhiTextureType::metalic_roughness) ? bindless_handles[rhiTextureType::metalic_roughness].index : 0,
                .base_sam_index = bindless_handles.count(rhiTextureType::base_color_sampler) ? bindless_handles[rhiTextureType::base_color_sampler].index : 0,
                .norm_sam_index = bindless_handles.count(rhiTextureType::normal_sampler) ? bindless_handles[rhiTextureType::normal_sampler].index : 0,
                .m_r_sam_index = bindless_handles.count(rhiTextureType::metalic_roughness_sampler) ? bindless_handles[rhiTextureType::metalic_roughness_sampler].index : 0,
                .alpha_cutoff = mat.alpha_cutoff,
                .metalic_factor = mat.metalic_factor,
                .roughness_factor = mat.roughness_factor,
                .is_transluent = mat.is_translucent,
                .is_double_sided = mat.is_double_sided,
                .first_index = sub_mesh.first_index,
                .index_count = sub_mesh.index_count,
            };

            const u8 dt = static_cast<u8>(mat.is_translucent ? drawType::translucent : drawType::gbuffer);
            buckets[dt][key].push_back(inst);
            submesh_buckets[dt][key] = sub_mesh;
        }
    }

    for (auto type : enum_range_to_sentinel<drawType, drawType::count>())
    {
        const u8 dt = static_cast<u8>(type);
        if (buckets[dt].empty())
            continue;

        u32 running = 0;
        groups[dt].reserve(buckets[dt].size());

        for (auto& [key, insts] : buckets[dt])
        {
            const rhiRenderResource::subMesh& sm = submesh_buckets[dt][key];

            const u32 first_instance = static_cast<u32>(instances[dt].size());
            instances[dt].insert(instances[dt].end(), insts.begin(), insts.end());

            meshletDrawParams param;
            param.first_meshlet = sm.first_meshlet;
            param.meshlet_count = sm.meshlet_count;
            param.first_instance = first_instance;
            param.instance_count = static_cast<u32>(insts.size());

            meshlet_draw_params[dt].push_back(param);
            meshlet_indirect_args[dt].push_back(rhiDrawMeshShaderIndirect{
                .groupcount_x = sm.meshlet_count,
                .groupcount_y = param.instance_count,
                .groupcount_z = 1
                });

            const groupRecord rec{
                .vbo = key.vbo,
                .ibo = key.ibo,
                .first_cmd = running,
                .cmd_count = 1,
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
            groups[dt].push_back(rec);

            if (type == drawType::gbuffer)
                double_sided.emplace(running, key.is_double_sided);

            running += rec.cmd_count;
        }

        const u32 instance_bytes = static_cast<u32>(instances[dt].size() * sizeof(instanceData));
        if (instance_bytes)
        {
            render_shared.create_or_resize_buffer(instance_buffer[dt], instance_bytes, rhiBufferUsage::storage | rhiBufferUsage::transfer_dst, rhiMem::auto_device);
            render_shared.upload_to_device(instance_buffer[dt].get(), instances[dt].data(), instance_bytes);
            render_shared.buffer_barrier(instance_buffer[dt].get(), {
                .src_stage = rhiPipelineStage::copy,
                .dst_stage = rhiPipelineStage::mesh_shader,
                .src_access = rhiAccessFlags::transfer_write,
                .dst_access = rhiAccessFlags::shader_read | rhiAccessFlags::shader_storage_read,
                .offset = 0,
                .size = instance_bytes,
                .src_queue = render_shared.context->get_queue_family_index(rhiQueueType::graphics),
                .dst_queue = render_shared.context->get_queue_family_index(rhiQueueType::transfer) });
        }
        else
        {
            instance_buffer[dt].reset();
        }

        const u32 params_bytes = static_cast<u32>(meshlet_draw_params[dt].size() * sizeof(meshletDrawParams));
        if (params_bytes)
        {
            render_shared.create_or_resize_buffer(meshlet_draw_buffer[dt], params_bytes, rhiBufferUsage::storage | rhiBufferUsage::transfer_dst, rhiMem::auto_device);
            render_shared.upload_to_device(meshlet_draw_buffer[dt].get(), meshlet_draw_params[dt].data(), params_bytes);
            render_shared.buffer_barrier(meshlet_draw_buffer[dt].get(), {
                .src_stage = rhiPipelineStage::copy,
                .dst_stage = rhiPipelineStage::mesh_shader,
                .src_access = rhiAccessFlags::transfer_write,
                .dst_access = rhiAccessFlags::shader_read | rhiAccessFlags::shader_storage_read,
                .offset = 0,
                .size = params_bytes,
                .src_queue = render_shared.context->get_queue_family_index(rhiQueueType::graphics),
                .dst_queue = render_shared.context->get_queue_family_index(rhiQueueType::transfer) });
        }
        else
        {
            meshlet_draw_buffer[dt].reset();
        }

        const u32 indirect_bytes = static_cast<u32>(meshlet_indirect_args[dt].size() * sizeof(rhiDrawMeshShaderIndirect));
        if (indirect_bytes)
        {
            render_shared.create_or_resize_buffer(indirect_buffer[dt], indirect_bytes, rhiBufferUsage::indirect | rhiBufferUsage::transfer_dst, rhiMem::auto_device);
            render_shared.upload_to_device(indirect_buffer[dt].get(), meshlet_indirect_args[dt].data(), indirect_bytes);
            render_shared.buffer_barrier(indirect_buffer[dt].get(), {
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
            indirect_buffer[dt].reset();
        }
    }

    shadow_pass.update_elements(&groups, &instance_buffer, &indirect_buffer);
    gbuffer_pass.update_elements(&groups, &instance_buffer, &meshlet_draw_buffer, &indirect_buffer);
    translucent_pass.update_elements(&groups, &instance_buffer, &indirect_buffer);
}

void renderer::build_meshlet_global_vertices(meshActor* actor, meshlet::buildOut& out)
{
    if (!cache.contains(actor->get_mesh_hash()))
        return;

    auto& rhi_resource = cache[actor->get_mesh_hash()];
    auto raw_data = rhi_resource->get_raw_data();
    ASSERT(raw_data);

    const u32 base = static_cast<u32>(out.position.size());

    out.position.reserve(base + raw_data->vertices.size());
    out.normal.reserve(base + raw_data->vertices.size());
    out.tangent.reserve(base + raw_data->vertices.size());
    out.uv.reserve(base + raw_data->vertices.size());

    // packing vertex info
    for (const auto& v : raw_data->vertices)
    {
        // pos(float4, w=1)
        out.position.push_back({ v.position.x, v.position.y, v.position.z, 1.0f });

        // normal/tangent (1010102 SNORM + handedness)
        vec3 tan = { v.tangent.x, v.tangent.y, v.tangent.z };
        const f32 dot_nt = glm::dot(v.normal, tan);
        tan = { tan.x - v.normal.x * dot_nt, tan.y - v.normal.y * dot_nt, tan.z - v.normal.z * dot_nt };

        f32 handed_f = (std::isfinite(v.tangent.w) && std::fabs(v.tangent.w) > 0.5f) ? std::copysign(1.f, v.tangent.w) : 1.f;
        i32 handed_i = (handed_f > 0.f) ? 1 : 0;

        out.normal.push_back(pack_1010102_snorm(v.normal.x, v.normal.y, v.normal.z, 0));
        out.tangent.push_back(pack_1010102_snorm(tan.x, tan.y, tan.z, handed_i));
        out.uv.push_back(pack_half2x16(v.uv.x, v.uv.y));
    }

    // submesh to meshlet
    for (auto& sm : rhi_resource->get_submeshes_mutable())
    {
        const u32 first = static_cast<u32>(out.meshlets.size());
        for (const auto& m : *sm.meshlets)
        {
            const meshletHeader h{
                .vertex_count = m.vertex_count,
                .prim_count = m.triangle_count,
                .vertex_offset = static_cast<u32>(out.meshlet_vertex_index.size()),
                .prim_byte_offset = static_cast<u32>(out.meshlet_tribytes.size())
            };

            for (u32 i = 0; i < m.vertex_count; ++i)
            {
                const auto mesh_local = (*sm.meshlet_vertices)[m.vertex_offset + i];
                out.meshlet_vertex_index.push_back(base + mesh_local);
            }

            out.meshlet_tribytes.insert(out.meshlet_tribytes.end(),
                (*sm.meshlet_triangles).begin() + m.triangle_offset,
                (*sm.meshlet_triangles).begin() + m.triangle_offset + (m.triangle_count * 3));

            out.meshlets.push_back(h);
        }
        sm.first_meshlet = first;
        sm.meshlet_count = static_cast<u32>(sm.meshlets->size());
    }
}

void renderer::build_meshlet_ssbo(const meshlet::buildOut* out)
{
    auto upload = [&](std::unique_ptr<rhiBuffer>& buf, const void* src,  const u32 bytes)
        {
            render_shared.create_or_resize_buffer(buf, bytes, rhiBufferUsage::storage | rhiBufferUsage::transfer_dst, rhiMem::auto_device);
            render_shared.upload_to_device(buf.get(), src, bytes);
            render_shared.buffer_barrier(buf.get(), {
                .src_stage = rhiPipelineStage::copy,
                .dst_stage = rhiPipelineStage::mesh_shader,
                .src_access = rhiAccessFlags::transfer_write,
                .dst_access = rhiAccessFlags::shader_read | rhiAccessFlags::shader_storage_read,
                .offset = 0,
                .size = bytes,
                .src_queue = render_shared.context->get_queue_family_index(rhiQueueType::graphics),
                .dst_queue = render_shared.context->get_queue_family_index(rhiQueueType::transfer) });
        };
    upload(meshlet_ssbo.pos, out->position.data(), static_cast<u32>(out->position.size()) * sizeof(vec4));
    upload(meshlet_ssbo.norm, out->normal.data(), static_cast<u32>(out->normal.size()) * sizeof(u32));
    upload(meshlet_ssbo.tan, out->tangent.data(), static_cast<u32>(out->tangent.size()) * sizeof(u32));
    upload(meshlet_ssbo.uv, out->uv.data(), static_cast<u32>(out->uv.size()) * sizeof(u32));

    upload(meshlet_ssbo.header, out->meshlets.data(), static_cast<u32>(out->meshlets.size()) * sizeof(meshletHeader));
    upload(meshlet_ssbo.vert_indices, out->meshlet_vertex_index.data(), static_cast<u32>(out->meshlet_vertex_index.size()) * sizeof(u32));
    // align 4byte
    u32 sz = static_cast<u32>(out->meshlet_tribytes.size()) * sizeof(u8);
    sz = (sz + 3) & ~3;
    upload(meshlet_ssbo.tri_bytes, out->meshlet_tribytes.data(), sz);
}

#else
void renderer::build(scene* s, rhiDeviceContext* context)
{
    std::ranges::for_each(instances, [](std::vector<instanceData>& args)
        {
            args.clear();
        });
    std::ranges::for_each(indirect_args, [](std::vector<rhiDrawIndexedIndirect>& args)
        {
            args.clear();
        });
    std::ranges::for_each(groups, [](std::vector<groupRecord>& args)
        {
            args.clear();
        });
    std::array<std::unordered_map<drawGroupKey, std::vector<instanceData>, drawGroupKeyHash>, draw_type_count> buckets;
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

            std::unordered_map<rhiTextureType, rhiBindlessHandle> bindless_handles;
            rhiTexture* base = mat.base_color.get();
            if (base)
                bindless_handles.emplace(rhiTextureType::base_color, bindless_table->register_sampled_image(base));
            rhiTexture* norm = mat.norm_color.get();
            if (norm)
                bindless_handles.emplace(rhiTextureType::normal, bindless_table->register_sampled_image(norm));
            rhiTexture* mr = mat.m_r_color.get();
            if (mr)
                bindless_handles.emplace(rhiTextureType::metalic_roughness, bindless_table->register_sampled_image(mr));
            rhiSampler* base_sam = mat.base_sampler.get();
            if (base_sam)
                bindless_handles.emplace(rhiTextureType::base_color_sampler, bindless_table->register_sampler(base_sam));
            rhiSampler* norm_sam = mat.norm_sampler.get();
            if (norm_sam)
                bindless_handles.emplace(rhiTextureType::normal_sampler, bindless_table->register_sampler(norm_sam));
            rhiSampler* mr_sam = mat.m_r_sampler.get();
            if (mr_sam)
                bindless_handles.emplace(rhiTextureType::metalic_roughness_sampler, bindless_table->register_sampler(mr_sam));

            const auto submesh_mat = actor_mat * sub_mesh.model;
            instanceData inst{
                .model = submesh_mat,
                .normal_mat = glm::transpose(glm::inverse(submesh_mat))
            };

            const drawGroupKey key
            {
                .vbo = vbo,
                .ibo = ibo,
                .base_color_index = bindless_handles.contains(rhiTextureType::base_color) ? bindless_handles[rhiTextureType::base_color].index : 0,
                .norm_color_index = bindless_handles.contains(rhiTextureType::normal) ? bindless_handles[rhiTextureType::normal].index : 0,
                .m_r_color_index = bindless_handles.contains(rhiTextureType::metalic_roughness) ? bindless_handles[rhiTextureType::metalic_roughness].index : 0,
                .base_sam_index = bindless_handles.contains(rhiTextureType::base_color_sampler) ? bindless_handles[rhiTextureType::base_color_sampler].index : 0,
                .norm_sam_index = bindless_handles.contains(rhiTextureType::normal_sampler) ? bindless_handles[rhiTextureType::normal_sampler].index : 0,
                .m_r_sam_index = bindless_handles.contains(rhiTextureType::metalic_roughness_sampler) ? bindless_handles[rhiTextureType::metalic_roughness_sampler].index : 0,
                .alpha_cutoff = mat.alpha_cutoff,
                .metalic_factor = mat.metalic_factor,
                .roughness_factor = mat.roughness_factor,
                .is_transluent = mat.is_translucent,
                .is_double_sided = mat.is_double_sided,
                .first_index = sub_mesh.first_index,
                .index_count = sub_mesh.index_count,
            };

            const u8 draw_type = static_cast<u8>(mat.is_translucent ? drawType::translucent : drawType::gbuffer);
            buckets[draw_type][key].push_back(inst);
        }
    }

    // sort indirect_args
    for (auto type : enum_range_to_sentinel<drawType, drawType::count>())
    {
        const auto draw_type = static_cast<u8>(type);
        if (buckets[draw_type].empty())
            continue;

        u32 running = 0;
        groups[draw_type].reserve(buckets[draw_type].size());
        for (auto& [key, insts] : buckets[draw_type])
        {
            const u32 first_instance = static_cast<u32>(instances[draw_type].size());
            instances[draw_type].insert(instances[draw_type].end(), insts.begin(), insts.end());

            rhiDrawIndexedIndirect cmd{
                .index_count = key.index_count,
                .instance_count = static_cast<u32>(insts.size()),
                .first_index = key.first_index,
                .vertex_offset = 0,
                .first_instance = first_instance
            };

            const groupRecord group_record
            {
                .vbo = key.vbo,
                .ibo = key.ibo,
                .first_cmd = running,
                .cmd_count = 1,
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

            groups[draw_type].push_back(group_record);
            indirect_args[draw_type].push_back(cmd);
            if(type == drawType::gbuffer)
                double_sided.emplace(running, key.is_double_sided);
            running += group_record.cmd_count;
        }

        // create gpu buffer / resize
        const u32 instance_bytes = static_cast<u32>(instances[draw_type].size()) * sizeof(instanceData);
        const u32 indirect_bytes = static_cast<u32>(indirect_args[draw_type].size()) * sizeof(rhiDrawIndexedIndirect);
        if (instance_bytes)
        {
            render_shared.create_or_resize_buffer(instance_buffer[draw_type], instance_bytes, rhiBufferUsage::storage | rhiBufferUsage::transfer_dst, rhiMem::auto_device);
            render_shared.upload_to_device(instance_buffer[draw_type].get(), instances[draw_type].data(), instance_bytes);
            render_shared.buffer_barrier(instance_buffer[draw_type].get(), {
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
            instance_buffer[draw_type].reset();
        }

        if (indirect_bytes)
        {
            render_shared.create_or_resize_buffer(indirect_buffer[draw_type], indirect_bytes, rhiBufferUsage::indirect | rhiBufferUsage::transfer_dst, rhiMem::auto_device);
            render_shared.upload_to_device(indirect_buffer[draw_type].get(), indirect_args[draw_type].data(), indirect_bytes);
            render_shared.buffer_barrier(indirect_buffer[draw_type].get(), {
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
            indirect_buffer[draw_type].reset();
        }
    }

    shadow_pass.update_elements(&groups, &instance_buffer, &indirect_buffer);
    gbuffer_pass.update_elements(&groups, &instance_buffer, &indirect_buffer);
    translucent_pass.update_elements(&groups, &instance_buffer, &indirect_buffer);
    gbuffer_pass.update_double_sided_info(double_sided);
}
#endif

void renderer::create_ringbuffer(const u32 frame_size)
{
    global_ringbuffer.resize(frame_size);
    for (u32 index = 0; index < frame_size; ++index)
    {
        render_shared.create_or_resize_buffer(global_ringbuffer[index], sizeof(globalsCB), rhiBufferUsage::uniform | rhiBufferUsage::transfer_dst, rhiMem::auto_device);
    }
}

void renderer::notify_nextimage_index_to_drawpass(const u32 image_index)
{
    shadow_pass.frame(image_index);
    gbuffer_pass.frame(image_index);
    sky_pass.frame(image_index);
    lighting_pass.frame(image_index);
    translucent_pass.frame(image_index);
    oit_pass.frame(image_index);
    composite_pass.frame(image_index);
}