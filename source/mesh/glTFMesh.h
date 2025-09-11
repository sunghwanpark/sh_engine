#pragma once

#include "pch.h"

struct glTFVertex
{
	vec3 position;
	vec3 normal;
	vec2 uv;
	vec4 tangent;
	u64 hash(u64 h = 1469598103934665603ull) const
	{
		h = hash_combine(h, position);
		h = hash_combine(h, normal);
		h = hash_combine(h, uv);
		h = hash_combine(h, tangent);
		return h;
	}
};

enum class gltfFilter : u8
{
	nearest,
	linear
};

enum class gltfMipmap : u8
{
	nearest,
	linear
};

enum class gltfWrap : u8
{
	repeat,
	mirrored_repeat,
	clamp_to_edge
};

struct glTFSampler
{
	gltfFilter mag_filter;
	gltfFilter min_filter;
	gltfMipmap mipmap;
	gltfWrap wrap_u;
	gltfWrap wrap_v;
};

struct glTFSubmesh
{
	u32 firstIndex = 0;
	u32 indexCount = 0;
	
	std::string base_tex;
	std::string normal_tex;
	std::string metalic_roughness_tex;

	glTFSampler base_sampler;
	glTFSampler norm_sampler;
	glTFSampler m_r_sampler;

	f32 metalic_factor;
	f32 roughness_factor;

	bool is_masked = false;
	f32 alpha_cutoff;

	mat4 model;

	u64 hash(u64 h = 1469598103934665603ull) const
	{
		h = hash_combine(h, firstIndex);
		h = hash_combine(h, indexCount);
		h = hash_combine(h, base_tex);
		h = hash_combine(h, normal_tex);
		h = hash_combine(h, metalic_roughness_tex);
		h = hash_combine(h, is_masked);
		h = hash_combine(h, alpha_cutoff);
		return h;
	}
};

struct glTFMaterial
{
	std::string name;
};

class glTFMesh
{
public:
	glTFMesh() = default;
	glTFMesh(const std::string_view path);

	u64 hash() const
	{
		u64 h = 1469598103934665603ull;

		for (auto& v : vertices)
			h = v.hash(h);

		for (auto& i : indices)
			h = hash_combine(h, i);

		for (auto& sm : submeshes)
			h = sm.hash(h);

		return h;
	}

	std::vector<glTFVertex> vertices;
	std::vector<u32> indices;
	std::vector<glTFSubmesh> submeshes;
};
