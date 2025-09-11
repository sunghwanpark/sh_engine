#include "meshModelManager.h"
#include "mesh/glTFMesh.h"

std::weak_ptr<glTFMesh> meshModelManager::get_asset(std::string_view asset_path)
{
	const u64 hash = string_to_u64hash(asset_path);
	if (asset_cache.contains(hash))
	{
		return asset_cache[hash];
	}
	return {};
}

std::weak_ptr<glTFMesh> meshModelManager::get_or_create_asset(std::string_view asset_path)
{
	const u64 hash = string_to_u64hash(asset_path);
	if (asset_cache.contains(hash))
	{
		return asset_cache[hash];
	}
	return create_asset(asset_path);
}

std::weak_ptr<glTFMesh> meshModelManager::create_asset(std::string_view asset_path)
{
	std::shared_ptr<glTFMesh> asset = std::make_shared<glTFMesh>(asset_path);
	const u64 hash = string_to_u64hash(asset_path);
	asset_cache.emplace(hash, asset);

	return asset_cache[hash];
}