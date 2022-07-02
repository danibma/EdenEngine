#pragma once

#include "Graphics/D3D12/D3D12RHI.h"
#include "Scene/MeshSource.h"

namespace Eden
{
	class Skybox
	{
		std::unique_ptr<MeshSource> m_SkyboxCube;
		std::shared_ptr<Texture2D> m_SkyboxTexture;
		std::shared_ptr<Buffer> m_SkyboxDataCB;
		std::string m_SkyboxTexturePath = "assets/skyboxes/studio_garden.hdr";
		struct SkyboxData
		{
			glm::mat4 view_projection;
		} m_SkyboxData;

	public:
		Skybox() = default;
		Skybox(std::shared_ptr<D3D12RHI>& rhi, const char* texture_path);
		~Skybox();

		void Render(std::shared_ptr<D3D12RHI>& rhi, glm::mat4 view_project_matrix);
		void SetNewTexture(const char* texture_path);
		void UpdateNewTexture(std::shared_ptr<D3D12RHI>& rhi);
	};
}

