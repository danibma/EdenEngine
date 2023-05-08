#include "RHIMeshTest.h"
#include "imgui.h"
#include "Core/Application.h"

namespace Eden::Gfx::Tests 
{
	void RHIMeshTest::Init(Window* window)
	{
		RHICreate(window);

		m_Camera = Camera(window->GetWidth(), window->GetHeight());

		RenderPassDesc renderPassDesc = {};
		renderPassDesc.debugName = "Mesh Test Render Pass";
		renderPassDesc.bIsSwapchainTarget = true;
		renderPassDesc.bIsImguiPass = true;
		renderPassDesc.attachmentsFormats = { Format::kRGBA32_FLOAT, Format::Depth };
		renderPassDesc.width = window->GetWidth();
		renderPassDesc.height = window->GetHeight();
		m_MainRenderPass = RHICreateRenderPass(&renderPassDesc);

		PipelineDesc pipelineDesc = {};
		pipelineDesc.debugName = "Mesh Test Pipeline";
		pipelineDesc.programName = "tests/MeshTest";
		pipelineDesc.renderPass = m_MainRenderPass;
		m_MainPipeline = RHICreatePipeline(&pipelineDesc);

		m_MeshSource = MakeShared<MeshSource>();
		m_MeshSource->LoadGLTF("assets/Models/DamagedHelmet/DamagedHelmet.gltf");

		RHIEnableImGui();
	}

	void RHIMeshTest::Update()
	{
		RHIBeginRender();

		RHIBeginRenderPass(m_MainRenderPass);
		
		// Draw mesh
		RHIBindPipeline(m_MainPipeline);
		RHIBindVertexBuffer(m_MeshSource->meshVb);
		RHIBindIndexBuffer(m_MeshSource->meshIb);
		for (auto& mesh : m_MeshSource->meshes)
		{
			m_Camera.Update(Application::Get()->GetDeltaTime());
			glm::mat4 viewMatrix = glm::lookAtLH(m_Camera.position, m_Camera.position + m_Camera.front, m_Camera.up);
			glm::mat4 projectionMatrix = glm::perspectiveFovLH(glm::radians(70.0f), 1600.0f, 900.0f, 0.1f, 200.0f);
			glm::mat4 viewProjection = projectionMatrix * viewMatrix;
			RHIBindParameter("SceneData", &viewProjection, sizeof(glm::mat4));

			RHIBindParameter("Transform", &mesh->modelMatrix, sizeof(glm::mat4));

			for (auto& submesh : mesh->submeshes) 
			{
				RHIBindParameter("g_AlbedoMap", submesh->material.albedoMap);
				RHIBindParameter("g_EmissiveMap", submesh->material.emissiveMap);

				RHIDrawIndexed(submesh->indexCount, 1, submesh->indexStart);
			}
		}
		
		// ImGui
		ImGui::ShowDemoWindow();

		RHIEndRenderPass(m_MainRenderPass);


		RHIEndRender();
		RHIRender();
	}

	RHIMeshTest::~RHIMeshTest()
	{
		m_MainRenderPass.Reset();
		m_MainPipeline.Reset();
		m_VertexBuffer.Reset();

		m_MeshSource.Reset();

		RHIShutdown();
	}
}