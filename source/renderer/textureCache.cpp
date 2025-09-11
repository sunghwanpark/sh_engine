#include "textureCache.h"
#include "rhi/rhiTextureView.h"
#include "rhi/rhiDeviceContext.h"
#include "util/hash.h"

std::shared_ptr<rhiTexture> textureCache::get_or_create(std::string_view path)
{
	if (path.empty())
		return {};

	if (textures.contains(path))
		return textures[path];

	auto texture = context->create_texture_from_path(path);
	std::shared_ptr<rhiTexture> sp = std::move(texture);
	textures.emplace(path, sp);
	return textures[path];
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
	textures.clear();
	samplers.clear();
}