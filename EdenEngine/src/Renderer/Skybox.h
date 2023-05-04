#pragma once

#include <memory>
#include <string>
#include <glm/glm.hpp>

#include "RHI/DynamicRHI.h"

namespace Eden
{
	struct MeshSource;
	class Skybox
	{
		std::unique_ptr<MeshSource> m_SkyboxCube;
		TextureRef m_SkyboxTexture;
		glm::mat4 m_ViewProjection;
		std::string m_SkyboxTexturePath;
		bool m_bTextureNeedsReload = false;

	public:
		Skybox() = default;
		Skybox(const char* texturePath);
		~Skybox();

		// Loads the new skybox texture in case it changed
		void Prepare();

		void Render(glm::mat4 viewProjectMatrix);
		void SetNewTexture(const char* texturePath);
	};
}

