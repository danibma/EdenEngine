#pragma once

#include <string>
#include <glm/glm.hpp>

namespace Eden
{
	struct TagComponent
	{
		std::string tag;
	};

	struct TransformComponent
	{
		glm::vec3 translation = glm::vec3(1.0f);
	};
}
