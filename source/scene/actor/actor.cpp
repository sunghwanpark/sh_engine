#include "actor.h"
#include "component/transformComponent.h"

actor::actor()
{
	transform = std::make_unique<transformComponent>();
}

actor::~actor()
{
	transform.reset();
}

void actor::add_scene()
{
}

void actor::set_position(vec3 pos)
{
	transform->position = pos;
}

void actor::set_rotation(quat rot)
{
	transform->rotation = rot;
}

void actor::set_scale(vec3 scale)
{
	transform->scale = scale;
}