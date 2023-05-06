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
		m_SkyboxTexture = RHICreateTexture(texturePath, false);
	}

	Skybox::~Skybox()
	{
	}

	void Skybox::Prepare()
	{
		if (m_bTextureNeedsReload)
		{
			m_SkyboxTexture = RHICreateTexture(m_SkyboxTexturePath, false);
			m_bTextureNeedsReload = false;
		}
	}

	void Skybox::Render(glm::mat4 viewProjectMatrix)
	{
		RHIBindVertexBuffer(m_SkyboxCube->meshVb);
		RHIBindIndexBuffer(m_SkyboxCube->meshIb);
		for (auto& mesh : m_SkyboxCube->meshes)
		{
			m_ViewProjection = viewProjectMatrix;

			RHIBindParameter("SkyboxData", &m_ViewProjection, sizeof(glm::mat4));
			for (auto& submesh : mesh->submeshes)
			{
				RHIBindParameter("g_CubemapTexture", m_SkyboxTexture);
				RHIDrawIndexed(submesh->indexCount, 1, submesh->indexStart);
			}
		}
	}

	void Skybox::SetNewTexture(const char* texturePath)
	{
		m_SkyboxTexturePath = texturePath;
		m_bTextureNeedsReload = true;
	}
}
