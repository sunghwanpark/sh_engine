#pragma once

#include "pch.h"
#include "rhi/rhiSampler.h"

class rhiTexture;
class rhiDeviceContext;
class textureCache
{
public:
	textureCache(rhiDeviceContext* context) : context(context) {}

public:
	std::shared_ptr<rhiTexture> get_or_create(std::string_view path);
	std::shared_ptr<rhiSampler> get_or_create(const rhiSamplerDesc& desc);
	void clear();

public:
	rhiDeviceContext* context;
	std::unordered_map<std::string_view, std::shared_ptr<rhiTexture>> textures;
	std::unordered_map<rhiSamplerKey, std::shared_ptr<rhiSampler>, rhiSamplerKeyHash> samplers;
};