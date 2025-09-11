#pragma once

#include "pch.h"

struct transformComponent;
class actor
{
public:
	actor();
	virtual ~actor();

public:
	virtual void add_scene();
	void set_position(vec3 pos);
	void set_rotation(quat rot);
	void set_scale(vec3 scale);

public:
	std::unique_ptr<transformComponent> transform;
};
