#pragma once

#include "pch.h"

struct transformComponent
{
	vec3 position;
	quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
	vec3 scale{ 1.0f, 1.0f, 1.0f };

	const mat4 matrix() const
	{
		glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
		glm::quat q = glm::normalize(rotation);
		glm::mat4 R = glm::mat4_cast(q);
		glm::vec3 s = glm::max(scale, glm::vec3(1e-6f));
		glm::mat4 S = glm::scale(glm::mat4(1.0f), s);
		return T * R * S;
	}
};
