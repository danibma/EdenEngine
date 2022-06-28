#pragma once

#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include "MeshSource.h"

namespace Eden
{
	// Steps to create a new component:
	// 1- Create the struct here
	// 2- Draw the component property by calling DrawComponentProperty inside UI_EntityProperties
	// 3- Add a new menu item inside the "addc_popup"
	// 4- Add a new item in SerializeEntity in the SceneSerializer

	struct TagComponent
	{
		std::string tag;
	};

	struct TransformComponent
	{
		glm::vec3 translation	= glm::vec3(0.0f);
		glm::vec3 rotation		= glm::vec3(0.0f);
		glm::vec3 scale			= glm::vec3(1.0f);

		glm::mat4 GetTransform()
		{
			glm::mat4 t = glm::translate(glm::mat4(1.0f), translation);
			glm::mat4 r = glm::toMat4(glm::quat(rotation));
			glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);

			return t * r * s;
		}
	};

	struct MeshComponent
	{
		std::shared_ptr<MeshSource> mesh_source = std::make_shared<MeshSource>();
		std::string mesh_path;

		void LoadMeshSource(D3D12RHI* rhi, std::filesystem::path path = "")
		{
			if (!path.empty())
				mesh_path = path.string();
			mesh_source->LoadGLTF(rhi, mesh_path);
		}
	};

	struct PointLightComponent
	{
		glm::vec4 color = glm::vec4(1.0f);
		glm::vec4 position;
		float constant_value = 1.0f;
		float linear_value = 0.09f;
		float quadratic_value = 0.032f;
	};

	struct DirectionalLightComponent
	{
		glm::vec4 direction;
		float intensity = 1.0f;
	};
}
