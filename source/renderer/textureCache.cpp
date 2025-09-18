#include "textureCache.h"
#include "rhi/rhiTextureView.h"
#include "rhi/rhiDeviceContext.h"
#include "util/hash.h"

std::shared_ptr<rhiTexture> textureCache::get_or_create(std::string_view path, bool srgb)
{
	if (path.empty())
		return {};

	auto& cache = srgb ? srgb_textures : linear_textures;
	std::string key(path);
	if (auto it = cache.find(key); it != cache.end())
		return it->second;

	auto texture = context->create_texture_from_path(path, false, srgb);
	std::shared_ptr<rhiTexture> sp = std::move(texture);
	cache.emplace(std::move(key), sp);
	return sp;
}

std::shared_ptr<rhiSampler> textureCache::get_or_create(const rhiSamplerDesc& desc)
{
	const rhiSamplerKey key = desc;
	if (samplers.contains(key))
		return samplers[key];

	auto sampler = context->create_sampler(desc);
	std::shared_ptr<rhiSampler> sp = std::move(sampler);
	samplers.emplace(key, sp);
	return samplers[key];
}

void textureCache::clear()
{
	srgb_textures.clear();
	linear_textures.clear();
	samplers.clear();
}
