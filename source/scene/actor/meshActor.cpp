#include "meshActor.h"
#include "scene/actor/component/meshComponent.h"

meshActor::meshActor(std::weak_ptr<meshModelManager> mesh_model_manager, const std::string_view mesh_path)
	: actor(), mesh_path(mesh_path)
{
	mesh = std::make_unique<meshComponent>(mesh_model_manager, mesh_path);
}

meshActor::~meshActor()
{
	mesh.release();
}

void meshActor::add_scene()
{
}

const u64 meshActor::get_mesh_hash() const
{
	return mesh->get_mesh_hash();
}