#pragma once

#include <memory>
#include <string>
#include <glm/glm.hpp>

#include "Graphics/RHI.h"

namespace Eden
{
	struct MeshSource;
	class Skybox
	{
		std::unique_ptr<MeshSource> m_SkyboxCube;
		Texture m_SkyboxTexture;
		glm::mat4 m_ViewProjection;

	public:
		Skybox() = default;
		Skybox(const char* texturePath);
		~Skybox();

		void Render(glm::mat4 viewProjectMatrix);
		void SetNewTexture(const char* texturePath);
	};
}

