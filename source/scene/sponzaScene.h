#pragma once

#include "pch.h"
#include "scene/scene.h"

class sponzaScene : public scene
{
protected:
	void create_actor() final override;
};
