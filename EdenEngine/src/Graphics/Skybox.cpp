#include "Skybox.h"

#include "Graphics/RHI.h"
#include "Scene/MeshSource.h"

namespace Eden
{

	Skybox::Skybox(std::shared_ptr<IRHI>& rhi, const char* texture_path)
		: m_SkyboxTexturePath(texture_path)
	{
		m_ViewProjection = glm::mat4(1.0f);

		m_SkyboxCube = std::make_unique<MeshSource>();
		m_SkyboxCube->LoadGLTF(rhi, "assets/models/basic/cube.glb");
		m_SkyboxTexture = rhi->CreateTexture(m_SkyboxTexturePath, false);
	}

	Skybox::~Skybox()
	{
	}

	void Skybox::Render(std::shared_ptr<IRHI>& rhi, glm::mat4 view_project_matrix)
	{
		rhi->BindVertexBuffer(m_SkyboxCube->mesh_vb);
		rhi->BindIndexBuffer(m_SkyboxCube->mesh_ib);
		for (auto& mesh : m_SkyboxCube->meshes)
		{
			m_ViewProjection = view_project_matrix;

			rhi->BindParameter("SkyboxData", &m_ViewProjection, sizeof(glm::mat4));
			for (auto& submesh : mesh->submeshes)
			{
				rhi->BindParameter("g_cubemapTexture", m_SkyboxTexture);
				rhi->DrawIndexed(submesh->index_count, 1, submesh->index_start);
			}
		}
	}

	void Skybox::SetNewTexture(const char* texture_path)
	{
		m_SkyboxTexturePath = texture_path;
	}

	void Skybox::UpdateNewTexture(std::shared_ptr<IRHI>& rhi)
	{
		m_SkyboxTexture = rhi->CreateTexture(m_SkyboxTexturePath, false);
	}

}
