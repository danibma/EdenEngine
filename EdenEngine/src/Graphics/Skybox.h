#pragma once

#include <memory>
#include <string>
#include <glm/glm.hpp>

#include "Graphics/RHI.h"

namespace Eden
{
	class IRHI;
	struct MeshSource;
	class Skybox
	{
		std::unique_ptr<MeshSource> m_SkyboxCube;
		Texture m_SkyboxTexture;
		std::string m_SkyboxTexturePath;
		glm::mat4 m_ViewProjection;

	public:
		Skybox() = default;
		Skybox(std::shared_ptr<IRHI>& rhi, const char* texturePath);
		~Skybox();

		void Render(std::shared_ptr<IRHI>& rhi, glm::mat4 viewProjectMatrix);
		void SetNewTexture(const char* texturePath);
		void UpdateNewTexture(std::shared_ptr<IRHI>& rhi);
	};
}

