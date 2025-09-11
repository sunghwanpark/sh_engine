#pragma once

#include "pch.h"
#include "actor.h"

class meshComponent;
class meshModelManager;
class meshActor : public actor
{
public:
	meshActor(std::weak_ptr<meshModelManager> mesh_model_manager, const std::string_view mesh_path);
	virtual ~meshActor();

public:
	const u64 get_mesh_hash() const;
	void add_scene() override;

public:
	std::string_view mesh_path;
	std::unique_ptr<meshComponent> mesh;
};
