#include "Skybox.h"

#include "Scene/MeshSource.h"

namespace Eden
{

	Skybox::Skybox(std::shared_ptr<IRHI>& rhi, const char* texturePath)
		: m_SkyboxTexturePath(texturePath)
	{
		m_ViewProjection = glm::mat4(1.0f);

		m_SkyboxCube = std::make_unique<MeshSource>();
		m_SkyboxCube->LoadGLTF(rhi, "assets/models/basic/cube.glb");
		rhi->CreateTexture(&m_SkyboxTexture, m_SkyboxTexturePath, false);
	}

	Skybox::~Skybox()
	{
	}

	void Skybox::Render(std::shared_ptr<IRHI>& rhi, glm::mat4 viewProjectMatrix)
	{
		rhi->BindVertexBuffer(&m_SkyboxCube->meshVb);
		rhi->BindIndexBuffer(&m_SkyboxCube->meshIb);
		for (auto& mesh : m_SkyboxCube->meshes)
		{
			m_ViewProjection = viewProjectMatrix;

			rhi->BindParameter("SkyboxData", &m_ViewProjection, sizeof(glm::mat4));
			for (auto& submesh : mesh->submeshes)
			{
				rhi->BindParameter("g_CubemapTexture", &m_SkyboxTexture);
				rhi->DrawIndexed(submesh->indexCount, 1, submesh->indexStart);
			}
		}
	}

	void Skybox::SetNewTexture(const char* texturePath)
	{
		m_SkyboxTexturePath = texturePath;
	}

	void Skybox::UpdateNewTexture(std::shared_ptr<IRHI>& rhi)
	{
		rhi->CreateTexture(&m_SkyboxTexture, m_SkyboxTexturePath, false);
	}

}
