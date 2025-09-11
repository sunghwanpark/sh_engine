#pragma once

#include "pch.h"

class actor;
class camera;
class meshModelManager;
class directionalLightActor;
class scene
{
public:
	scene();
	~scene();

public:
	virtual void start_scene(GLFWwindow* window);
	virtual void update(GLFWwindow* window, float delta);

	std::span<std::unique_ptr<actor>> get_actors();
	std::shared_ptr<meshModelManager> get_mesh_manager() const;
	camera* get_camera() { return _camera.get(); }
	directionalLightActor* get_directional_light() { return directional_light.get(); }

protected:
	virtual void create_actor();

protected:
	std::vector<std::unique_ptr<actor>> actors;
	std::shared_ptr<meshModelManager> mesh_model_manager;
	std::unique_ptr<camera> _camera;
	std::unique_ptr<directionalLightActor> directional_light;
};
