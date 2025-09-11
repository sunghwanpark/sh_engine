#pragma once

#include "pch.h"
#include "lightActor.h"

class directionalLightActor : public lightActor
{
public:
	f32 get_light_intensity() const { return intensity; }
	vec3 get_lightcolor() const { return color; }
	i32 get_cascade_count() const { return cascade_count; }

private:
	f32 intensity = 10.f;
	vec3 color = { 1.f, 1.f, 1.f };
	i32 cascade_count = 4;
};
