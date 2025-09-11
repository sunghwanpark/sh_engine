#include "meshComponent.h"
#include "mesh/meshModelManager.h"
#include "mesh/glTFMesh.h"

meshComponent::meshComponent(std::weak_ptr<meshModelManager> mesh_model_manager, std::string_view mesh_path)
	: mesh_path(mesh_path)
{
	if (auto ptr = mesh_model_manager.lock())
	{
		mesh = ptr->get_or_create_asset(mesh_path);
	}
}

const u64 meshComponent::get_mesh_hash() const
{
	if (auto raw = mesh.lock())
	{
		return raw->hash();
	}
	return -1;
}