#include "scene.h"
#include "camera.h"
#include "light/directionalLightActor.h"
#include "actor/actor.h"
#include "mesh/meshModelManager.h"

scene::scene()
{
	mesh_model_manager = std::make_shared<meshModelManager>();
}

scene::~scene()
{
	mesh_model_manager.reset();
	std::ranges::for_each(actors, [&](std::unique_ptr<actor>& actor)
		{
			actor.release();
		});
}

std::span<std::unique_ptr<actor>> scene::get_actors()
{
	return actors;
}

std::shared_ptr<meshModelManager> scene::get_mesh_manager() const
{
	return mesh_model_manager;
}

void scene::start_scene(GLFWwindow* window)
{
	create_actor();
	_camera = std::make_unique<camera>(window);
}

void scene::create_actor()
{
	directional_light = std::make_unique<directionalLightActor>();
}

void scene::update(GLFWwindow* window, float delta)
{
	_camera->process_input(window, delta);
}
