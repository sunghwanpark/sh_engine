#pragma once

#include "pch.h"

class glTFMesh;
class meshModelManager;
class rhiRenderResource;
class meshComponent
{
public:
	meshComponent(std::weak_ptr<meshModelManager> mesh_model_manager, std::string_view mesh_path);
public:
	const u64 get_mesh_hash() const;

private:
	std::string_view mesh_path;
	std::weak_ptr<glTFMesh> mesh;
};
