#pragma once
#include "Scene/MeshSource.h"

namespace Eden
{
	struct DirectionalLight
	{
		struct DirectionalLightData
		{
			glm::vec4 direction;
			float intensity = 1.0f;
		} data;
	};
}

