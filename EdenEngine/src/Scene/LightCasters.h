#pragma once
#include "Scene/Model.h"

namespace Eden
{
	struct PointLight
	{
		struct PointLightData
		{
			glm::vec4 color;
			glm::vec4 position;
			float constant_value = 1.0f;
			float linear_value = 0.09f;
			float quadratic_value = 0.032f;
			float padding; // no use
		} data;

		// This data is used to draw the point light
		Model model;
		Buffer light_color_buffer;

		void Destroy()
		{
			model.Destroy();
			light_color_buffer.Release();
		}
	};

	struct DirectionalLight
	{
		struct DirectionalLightData
		{
			glm::vec4 direction;
			float intensity = 1.0f;
		} data;
	};
}

