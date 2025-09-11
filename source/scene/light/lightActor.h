#pragma once

#include "pch.h"
#include "../actor/actor.h"

class lightActor: public actor
{
public:
	void set_direction(vec3 dir) { light_direction = dir; }
	vec3 get_direction() const { return light_direction; }

private:
	vec3 light_direction;
};
