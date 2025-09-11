#include "sponzaScene.h"
#include "mesh/meshModelManager.h"
#include "actor/meshActor.h"
#include "light/directionalLightActor.h"

void sponzaScene::create_actor()
{
	scene::create_actor();
	auto sponza = std::make_unique<meshActor>(mesh_model_manager, "E:\\Sponza\\resource\\sponza\\Sponza.gltf");
	actors.push_back(std::move(sponza));

	directional_light->set_position(vec3(0.f, 50.f, 0.f));
	directional_light->set_direction(glm::normalize(vec3(0.f, -1.f, 0.05f)));
}
