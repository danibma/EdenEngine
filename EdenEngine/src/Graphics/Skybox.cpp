#include "Skybox.h"

namespace Eden
{

	Skybox::Skybox(std::shared_ptr<D3D12RHI>& rhi, const char* texture_path)
	{
		m_SkyboxTexturePath = texture_path;
		m_SkyboxData.view_projection = glm::mat4(1.0f);
		m_SkyboxDataCB = rhi->CreateBuffer<SkyboxData>(&m_SkyboxData, 1);
		m_SkyboxCube = std::make_unique<MeshSource>();
		m_SkyboxCube->LoadGLTF(rhi, "assets/models/basic/cube.glb");
		m_SkyboxTexture = rhi->CreateTexture2D(m_SkyboxTexturePath);
		m_SkyboxTexture->resource->SetName(L"Skybox texture");
	}

	Skybox::~Skybox()
	{
	}

	void Skybox::Render(std::shared_ptr<D3D12RHI>& rhi, glm::mat4 view_project_matrix)
	{
		rhi->BindVertexBuffer(m_SkyboxCube->mesh_vb);
		rhi->BindIndexBuffer(m_SkyboxCube->mesh_ib);
		for (auto& mesh : m_SkyboxCube->meshes)
		{
			m_SkyboxData.view_projection = view_project_matrix;
			rhi->UpdateBuffer<SkyboxData>(m_SkyboxDataCB, &m_SkyboxData, 1);
			rhi->BindParameter("SkyboxData", m_SkyboxDataCB);

			for (auto& submesh : mesh.submeshes)
			{
				rhi->BindParameter("g_cubemapTexture", m_SkyboxTexture);
				rhi->DrawIndexed(submesh.index_count, 1, submesh.index_start);
			}
		}
	}

	void Skybox::SetNewTexture(const char* texture_path)
	{
		m_SkyboxTexturePath = texture_path;
	}

	void Skybox::UpdateNewTexture(std::shared_ptr<D3D12RHI>& rhi)
	{
		m_SkyboxTexture->Release();
		m_SkyboxTexture = rhi->CreateTexture2D(m_SkyboxTexturePath);
	}

}
