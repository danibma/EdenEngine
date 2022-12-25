#include "Skybox.h"

#include "Scene/MeshSource.h"
#include "Renderer.h"

namespace Eden
{

	Skybox::Skybox(const char* texturePath)
	{
		m_ViewProjection = glm::mat4(1.0f);

		m_SkyboxCube = std::make_unique<MeshSource>();
		m_SkyboxCube->LoadGLTF("assets/models/basic/cube.glb");
		Renderer::CreateTexture(&m_SkyboxTexture, texturePath, false);
	}

	Skybox::~Skybox()
	{
	}

	void Skybox::Render(glm::mat4 viewProjectMatrix)
	{
		Renderer::BindVertexBuffer(&m_SkyboxCube->meshVb);
		Renderer::BindIndexBuffer(&m_SkyboxCube->meshIb);
		for (auto& mesh : m_SkyboxCube->meshes)
		{
			m_ViewProjection = viewProjectMatrix;

			Renderer::BindParameter("SkyboxData", &m_ViewProjection, sizeof(glm::mat4));
			for (auto& submesh : mesh->submeshes)
			{
				Renderer::BindParameter("g_CubemapTexture", &m_SkyboxTexture);
				Renderer::DrawIndexed(submesh->indexCount, 1, submesh->indexStart);
			}
		}
	}

	void Skybox::SetNewTexture(const char* texturePath)
	{
		Renderer::CreateTexture(&m_SkyboxTexture, texturePath, false);
	}
}
