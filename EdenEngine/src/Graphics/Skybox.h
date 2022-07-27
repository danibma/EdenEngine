#pragma once

#include <memory>
#include <string>
#include <glm/glm.hpp>

namespace Eden
{
	class IRHI;
	struct Texture;
	struct Buffer;
	struct MeshSource;
	class Skybox
	{
		std::unique_ptr<MeshSource> m_SkyboxCube;
		std::shared_ptr<Texture> m_SkyboxTexture;
		std::string m_SkyboxTexturePath;
		glm::mat4 m_ViewProjection;

	public:
		Skybox() = default;
		Skybox(std::shared_ptr<IRHI>& rhi, const char* texture_path);
		~Skybox();

		void Render(std::shared_ptr<IRHI>& rhi, glm::mat4 view_project_matrix);
		void SetNewTexture(const char* texture_path);
		void UpdateNewTexture(std::shared_ptr<IRHI>& rhi);
	};
}

