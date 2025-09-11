#include "rhiRenderResource.h"
#include "mesh/glTFMesh.h"
#include "renderer/renderShared.h"
#include "renderer/textureCache.h"
#include "rhi/rhiDeviceContext.h"
#include "rhi/rhiDefs.h"
#include "rhi/rhiBuffer.h"
#include "rhi/rhiCommandList.h"

namespace
{
	rhiSamplerDesc get_sampler_desc(const glTFSampler& s)
	{
		rhiSamplerDesc sampler_desc;
		switch (s.mag_filter)
		{
		case gltfFilter::nearest: sampler_desc.mag_filter = rhiFilter::nearest; break;
		case gltfFilter::linear: sampler_desc.mag_filter = rhiFilter::linear; break;
		}
		switch (s.min_filter)
		{
		case gltfFilter::nearest: sampler_desc.min_filter = rhiFilter::nearest; break;
		case gltfFilter::linear: sampler_desc.min_filter = rhiFilter::linear; break;
		}
		switch (s.mipmap)
		{
		case gltfMipmap::nearest: sampler_desc.mipmap_mode = rhiMipmapMode::nearest; break;
		case gltfMipmap::linear: sampler_desc.mipmap_mode = rhiMipmapMode::linear; break;
		}
		switch (s.wrap_u)
		{
		case gltfWrap::repeat: sampler_desc.address_u = rhiAddressMode::repeat; break;
		case gltfWrap::clamp_to_edge: sampler_desc.address_u = rhiAddressMode::clamp_to_edge; break;
		case gltfWrap::mirrored_repeat: sampler_desc.address_u = rhiAddressMode::mirror; break;
		}
		switch (s.wrap_v)
		{
		case gltfWrap::repeat: sampler_desc.address_v = rhiAddressMode::repeat; break;
		case gltfWrap::clamp_to_edge: sampler_desc.address_v = rhiAddressMode::clamp_to_edge; break;
		case gltfWrap::mirrored_repeat: sampler_desc.address_v = rhiAddressMode::mirror; break;
		}
		sampler_desc.address_w = sampler_desc.address_v;
		return sampler_desc;
	}
}

rhiRenderResource::rhiRenderResource(std::weak_ptr<glTFMesh> raw_data)
	: raw_data(raw_data)
{
}

void rhiRenderResource::upload(renderShared* rs, textureCache* tex_cache)
{
	if (is_uploaded())
		return;

	assert(!raw_data.expired());
	auto raw_data_ptr = raw_data.lock();
	const u64 vb_bytes = static_cast<u64>(raw_data_ptr->vertices.size()) * sizeof(glTFVertex);
	const u64 ib_bytes = static_cast<u64>(raw_data_ptr->indices.size()) * sizeof(u32);

	// device local buffer
	rhiBufferDesc vb_desc
	{
		.size = vb_bytes,
		.usage = rhiBufferUsage::vertex | rhiBufferUsage::transfer_dst,
		.memory = rhiMem::auto_device,
		.stride = sizeof(glTFVertex)
	};
	rhiBufferDesc ib_desc
	{
		.size = ib_bytes,
		.usage = rhiBufferUsage::index | rhiBufferUsage::transfer_dst,
		.memory = rhiMem::auto_device,
	};
	vbo = rs->context->create_buffer(vb_desc);
	ibo = rs->context->create_buffer(ib_desc);

	// staging buffer
	auto staging_buffer = [](const u64 bytes) -> rhiBufferDesc
		{
			return rhiBufferDesc
			{
				.size = bytes,
				.usage = rhiBufferUsage::transfer_src,
				.memory = rhiMem::auto_host
			};
		};
	auto staging_vbo = rs->context->create_buffer(staging_buffer(vb_bytes));
	auto staging_ibo = rs->context->create_buffer(staging_buffer(ib_bytes));

	// upload
	auto p = staging_vbo->map();
	std::memcpy(p, raw_data_ptr->vertices.data(), vb_bytes);
	staging_vbo->unmap();

	p = staging_ibo->map();
	std::memcpy(p, raw_data_ptr->indices.data(), ib_bytes);
	staging_ibo->unmap();

	auto cmd = rs->context->begin_onetime_commands();
	cmd->copy_buffer(staging_vbo.get(), 0, vbo.get(), 0, vb_bytes);
	cmd->copy_buffer(staging_ibo.get(), 0, ibo.get(), 0, ib_bytes);
	rs->context->submit_and_wait(std::move(cmd));

	rebuild_submeshes(tex_cache, raw_data_ptr.get());
}

void rhiRenderResource::rebuild_submeshes(textureCache* tex_cache, glTFMesh* raw_mesh)
{
	submeshes.clear();
	materials.reserve(raw_mesh->submeshes.size());
	for (u32 i = 0; i < raw_mesh->submeshes.size(); ++i) 
	{
		const auto& s = raw_mesh->submeshes[i];
		submeshes.push_back(subMesh
			{ 
				.first_index = s.firstIndex,
				.index_count = s.indexCount,
				.model = s.model,
				.material_slot = static_cast<u32>(i) 
			});
		
		materials.push_back(material{
			.base_color = tex_cache->get_or_create(s.base_tex),
			.norm_color = tex_cache->get_or_create(s.normal_tex),
			.m_r_color = tex_cache->get_or_create(s.metalic_roughness_tex),
			.base_sampler = tex_cache->get_or_create(get_sampler_desc(s.base_sampler)),
			.norm_sampler = tex_cache->get_or_create(get_sampler_desc(s.norm_sampler)),
			.m_r_sampler = tex_cache->get_or_create(get_sampler_desc(s.m_r_sampler)),
			.alpha_cutoff = s.alpha_cutoff,
			.metalic_factor = s.metalic_factor,
			.roughness_factor = s.roughness_factor,
			.is_masked = s.is_masked
			});
	}
}

rhiBuffer* rhiRenderResource::get_vbo() const
{
	return vbo.get();
}

rhiBuffer* rhiRenderResource::get_ibo() const
{
	return ibo.get();
}

const rhiRenderResource::material& rhiRenderResource::get_material(const i32 slot_index)
{
	if (slot_index >= materials.size())
		assert(false);

	return materials[slot_index];
}