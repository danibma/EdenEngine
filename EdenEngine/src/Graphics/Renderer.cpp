#include "Renderer.h"

#include "D3D12/D3D12RHI.h"
#include "Vulkan/VulkanRHI.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Core/Application.h"
#include "Profiling/Profiler.h"
#include "Core/CommandLine.h"

namespace Eden
{
	// RHI
	IRHI* g_RHI = nullptr;
	// Renderer Data
	RendererData* g_Data = nullptr;
	bool g_IsRendererInitialized = false;

	void Renderer::Init(Window* window)
	{
		g_Data = enew RendererData();
		g_Data->viewportSize = { static_cast<float>(window->GetWidth()), static_cast<float>(window->GetHeight()) };

		if (CommandLine::HasArg("vulkan"))
		{
			g_RHI = enew VulkanRHI();
		}
		else if (CommandLine::HasArg("d3d12"))
		{
			g_RHI = enew D3D12RHI();
		}
		else
		{
			g_RHI = enew D3D12RHI();
			ED_LOG_INFO("No Rendering was set through the command line arguments, choosing D3D12 by default!");
		}

		GfxResult error = g_RHI->Init(window);
		ensureMsgf(error == GfxResult::kNoError, "Failed to initialize RHI \"%s\"", Utils::APIToString(g_RHI->GetCurrentAPI()));

		g_Data->window = window;
		g_Data->camera = Camera(window->GetWidth(), window->GetHeight());

		g_Data->viewMatrix = glm::lookAtLH(g_Data->camera.position, g_Data->camera.position + g_Data->camera.front, g_Data->camera.up);
		g_Data->projectionMatrix = glm::perspectiveFovLH_ZO(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 200.0f);
		g_Data->sceneData.view = g_Data->viewMatrix;
		g_Data->sceneData.viewProjection = g_Data->projectionMatrix * g_Data->viewMatrix;
		g_Data->sceneData.viewPosition = glm::vec4(g_Data->camera.position, 1.0f);

		BufferDesc sceneDataDesc = {};
		sceneDataDesc.elementCount = 1;
		sceneDataDesc.stride = sizeof(RendererData::SceneData);
		error = g_RHI->CreateBuffer(&g_Data->sceneDataCB, &sceneDataDesc, &g_Data->sceneData);
		ensure(error == GfxResult::kNoError);

		g_Data->currentScene = enew Scene();
		std::string defaultScene = "assets/scenes/demo.escene";
		OpenScene(defaultScene);

		g_Data->skybox = MakeShared<Skybox>("assets/skyboxes/studio_garden.hdr");

		// Lights
		BufferDesc dl_desc = {};
		dl_desc.elementCount = g_Data->MAX_DIRECTIONAL_LIGHTS;
		dl_desc.stride = sizeof(DirectionalLightComponent);
		dl_desc.usage = BufferDesc::Storage;
		error = g_RHI->CreateBuffer(&g_Data->directionalLightsBuffer, &dl_desc, nullptr);
		ensure(error == GfxResult::kNoError);

		BufferDesc pl_desc = {};
		pl_desc.elementCount = g_Data->MAX_POINT_LIGHTS;
		pl_desc.stride = sizeof(PointLightComponent);
		pl_desc.usage = BufferDesc::Storage;
		error = g_RHI->CreateBuffer(&g_Data->pointLightsBuffer, &pl_desc, nullptr);
		ensure(error == GfxResult::kNoError);

		UpdateDirectionalLights();
		UpdatePointLights();

		// Object Picker
		{
			RenderPassDesc desc = {};
			desc.debugName = "ObjectPicker";
			desc.attachmentsFormats = { Format::kRGBA32_FLOAT, Format::Depth };
			desc.width = static_cast<uint32_t>(g_Data->viewportSize.x);
			desc.height = static_cast<uint32_t>(g_Data->viewportSize.y);
			desc.clearColor = glm::vec4(-1);
			error = g_RHI->CreateRenderPass(&g_Data->objectPickerPass, &desc);
			ensure(error == GfxResult::kNoError);

			PipelineDesc pipelineDesc = {};
			pipelineDesc.programName = "ObjectPicker";
			pipelineDesc.renderPass = &g_Data->objectPickerPass;
			error = g_RHI->CreatePipeline(&g_Data->pipelines["Object Picker"], &pipelineDesc);
			ensure(error == GfxResult::kNoError);
		}

		// Forward Pass
		{
			RenderPassDesc desc = {};
			desc.debugName = "ForwardPass";
			desc.attachmentsFormats = { Format::kRGBA32_FLOAT, Format::Depth };
			desc.width = static_cast<uint32_t>(g_Data->viewportSize.x);
			desc.height = static_cast<uint32_t>(g_Data->viewportSize.y);
			error = g_RHI->CreateRenderPass(&g_Data->forwardPass, &desc);
			ensure(error == GfxResult::kNoError);

			PipelineDesc forwardDesc = {};
			forwardDesc.bEnableBlending = true;
			forwardDesc.programName = "ForwardPass";
			forwardDesc.renderPass = &g_Data->forwardPass;
			error = g_RHI->CreatePipeline(&g_Data->pipelines["Forward Rendering"], &forwardDesc);
			ensure(error == GfxResult::kNoError);

			PipelineDesc skyboxDesc = {};
			skyboxDesc.cull_mode = CullMode::kNone;
			skyboxDesc.depthFunc = ComparisonFunc::kLessEqual;
			skyboxDesc.minDepth = 1.0f;
			skyboxDesc.programName = "Skybox";
			skyboxDesc.renderPass = &g_Data->forwardPass;
			error = g_RHI->CreatePipeline(&g_Data->pipelines["Skybox"], &skyboxDesc);
			ensure(error == GfxResult::kNoError);

		}

		// Deferred Pass
		{
			RenderPassDesc desc = {};
			desc.debugName = "DeferredBasePass";
			desc.attachmentsFormats = { Format::kRGBA32_FLOAT, Format::kRGBA32_FLOAT, Format::kRGBA32_FLOAT, Format::Depth };
			desc.width = static_cast<uint32_t>(g_Data->viewportSize.x);
			desc.height = static_cast<uint32_t>(g_Data->viewportSize.y);
			desc.clearColor = { 0.0f };
			error = g_RHI->CreateRenderPass(&g_Data->deferredBasePass, &desc);
			ensure(error == GfxResult::kNoError);

			PipelineDesc deferredBaseDesc = {};
			deferredBaseDesc.bEnableBlending = true;
			deferredBaseDesc.programName = "DeferredBasePass";
			deferredBaseDesc.renderPass = &g_Data->deferredBasePass;
			error = g_RHI->CreatePipeline(&g_Data->pipelines["Deferred Base Pass"], &deferredBaseDesc);
			ensure(error == GfxResult::kNoError);

			RenderPassDesc lightingDesc = {};
			desc.debugName = "DeferredLightingPass";
			desc.attachmentsFormats = { Format::kRGBA32_FLOAT, Format::Depth };
			desc.width = static_cast<uint32_t>(g_Data->viewportSize.x);
			desc.height = static_cast<uint32_t>(g_Data->viewportSize.y);
			error = g_RHI->CreateRenderPass(&g_Data->deferredLightingPass, &desc);

			PipelineDesc deferredLightingPass = {};
			deferredLightingPass.cull_mode = CullMode::kNone;
			deferredLightingPass.bEnableBlending = false;
			deferredLightingPass.programName = "DeferredLightingPass";
			deferredLightingPass.renderPass = &g_Data->deferredLightingPass;
			error = g_RHI->CreatePipeline(&g_Data->pipelines["Deferred Lighting Pass"], &deferredLightingPass);
			ensure(error == GfxResult::kNoError);
		}

		// Scene Composite
		{
			RenderPassDesc desc = {};
			desc.debugName = "SceneComposite";
			desc.attachmentsFormats = { Format::kRGBA32_FLOAT, Format::Depth };
#ifndef WITH_EDITOR
			desc.bIsSwapchainTarget = true;
#endif
			desc.width = static_cast<uint32_t>(g_Data->viewportSize.x);
			desc.height = static_cast<uint32_t>(g_Data->viewportSize.y);
			error = g_RHI->CreateRenderPass(&g_Data->sceneComposite, &desc);
			ensure(error == GfxResult::kNoError);
#ifndef WITH_EDITOR
			g_RHI->SetSwapchainTarget(&g_Data->sceneComposite);
#endif

			PipelineDesc sceneCompositeDesc = {};
			sceneCompositeDesc.cull_mode = CullMode::kNone;
			sceneCompositeDesc.programName = "SceneComposite";
			sceneCompositeDesc.renderPass = &g_Data->sceneComposite;
			error = g_RHI->CreatePipeline(&g_Data->pipelines["Scene Composite"], &sceneCompositeDesc);
			ensure(error == GfxResult::kNoError);
		}

		float quadVertices[] = {
			// positions   // texCoords
			-1.0f,  1.0f,   0.0f, 1.0f,
			-1.0f, -1.0f,   0.0f, 0.0f,
			 1.0f, -1.0f,  -1.0f, 0.0f,

			-1.0f,  1.0f,   0.0f, 1.0f,
			 1.0f, -1.0f,  -1.0f, 0.0f,
			 1.0f,  1.0f,  -1.0f, 1.0f
		};
		BufferDesc quadDesc = {};
		quadDesc.stride = sizeof(float) * 4;
		quadDesc.elementCount = 6;
		quadDesc.usage = BufferDesc::Vertex_Index;
		error = g_RHI->CreateBuffer(&g_Data->quadBuffer, &quadDesc, quadVertices);
		ensure(error == GfxResult::kNoError);

		BufferDesc sceneSettingsDesc = {};
		sceneSettingsDesc.elementCount = 1;
		sceneSettingsDesc.stride = sizeof(RendererData::SceneSettings);
		sceneSettingsDesc.usage = BufferDesc::Uniform;
		error = g_RHI->CreateBuffer(&g_Data->sceneSettingsBuffer, &sceneSettingsDesc, &g_Data->sceneSettings);
		ensure(error == GfxResult::kNoError);

		error = g_RHI->CreateGPUTimer(&g_Data->renderTimer);
		ensure(error == GfxResult::kNoError);

		ED_LOG_INFO("Renderer has been initialized!");
		g_IsRendererInitialized = true;
	}

	void Renderer::BeginRender()
	{
		PrepareScene();

		// Update camera and scene data
		g_Data->camera.Update(Application::Get()->GetDeltaTime());
		g_Data->viewMatrix = glm::lookAtLH(g_Data->camera.position, g_Data->camera.position + g_Data->camera.front, g_Data->camera.up);
		g_Data->projectionMatrix = glm::perspectiveFovLH(glm::radians(70.0f), g_Data->viewportSize.x, g_Data->viewportSize.y, 0.1f, 200.0f);
		g_Data->sceneData.view = g_Data->viewMatrix;
		g_Data->sceneData.viewProjection = g_Data->projectionMatrix * g_Data->viewMatrix;
		g_Data->sceneData.viewPosition = glm::vec4(g_Data->camera.position, 1.0f);
		GfxResult error = g_RHI->UpdateBufferData(&g_Data->sceneDataCB, &g_Data->sceneData);
		ensure(error == GfxResult::kNoError);

		UpdateDirectionalLights();
		UpdatePointLights();

		g_RHI->BeginRender();
	}

	void Renderer::Render()
	{
		ED_PROFILE_FUNCTION();

		g_RHI->BeginGPUTimer(&g_Data->renderTimer);

#if WITH_EDITOR
		ObjectPickerPass();
#endif
		if (g_Data->bIsDeferredEnabled)
			DeferredRenderingPass();
		else
			ForwardRenderingPass();
		SceneCompositePass();
	}

	void Renderer::EndRender()
	{
		g_RHI->EndGPUTimer(&g_Data->renderTimer);
		g_RHI->EndRender();
		g_RHI->Render();
	}

	void Renderer::Shutdown()
	{
		edelete g_Data->currentScene;
		edelete g_Data;

		g_RHI->Shutdown();
		edelete g_RHI;
	}

	bool Renderer::IsInitialized()
	{
		return g_IsRendererInitialized;
	}

	void Renderer::UpdatePointLights()
	{
		PointLightComponent emptyPl;
		emptyPl.color = glm::vec4(0);

		std::array<PointLightComponent, g_Data->MAX_POINT_LIGHTS> pointLightComponents;
		pointLightComponents.fill(emptyPl);

		auto pointLights = g_Data->currentScene->GetAllEntitiesWith<PointLightComponent>();
		for (int i = 0; i < pointLights.size(); ++i)
		{
			Entity e = { pointLights[i], g_Data->currentScene };
			PointLightComponent& pl = e.GetComponent<PointLightComponent>();
			pointLightComponents[i] = pl;
		}

		const void* data = pointLightComponents.data();
		GfxResult error = g_RHI->UpdateBufferData(&g_Data->pointLightsBuffer, data);
		ensure(error == GfxResult::kNoError);
	}

	void Renderer::UpdateDirectionalLights()
	{
		DirectionalLightComponent emptyDl;
		emptyDl.intensity = 0;

		std::array<DirectionalLightComponent, g_Data->MAX_DIRECTIONAL_LIGHTS> directional_lightComponents;
		directional_lightComponents.fill(emptyDl);

		auto directional_lights = g_Data->currentScene->GetAllEntitiesWith<DirectionalLightComponent>();
		for (int i = 0; i < directional_lights.size(); ++i)
		{
			Entity e = { directional_lights[i], g_Data->currentScene };
			DirectionalLightComponent& pl = e.GetComponent<DirectionalLightComponent>();
			directional_lightComponents[i] = pl;
		}

		//if (directional_lights.size() > 0)
		//	m_DirectionalLight = { directional_lights[0], m_CurrentScene };

		const void* data = directional_lightComponents.data();
		GfxResult error = g_RHI->UpdateBufferData(&g_Data->directionalLightsBuffer, data);
		ensure(error == GfxResult::kNoError);
	}
	
	void Renderer::ObjectPickerPass()
	{
		auto entitiesToRender = g_Data->currentScene->GetAllEntitiesWith<MeshComponent>();

		g_RHI->BeginRenderPass(&g_Data->objectPickerPass);
		g_RHI->BindPipeline(&g_Data->pipelines["Object Picker"]);
		for (auto entity : entitiesToRender)
		{
			Entity e = { entity, g_Data->currentScene };
			MeshSource* ms = e.GetComponent<MeshComponent>().meshSource.Get();
			if (!ms->bHasMesh)
				continue;
			TransformComponent tc = e.GetComponent<TransformComponent>();

			g_RHI->BindVertexBuffer(&ms->meshVb);
			g_RHI->BindIndexBuffer(&ms->meshIb);
			for (auto& mesh : ms->meshes)
			{
				auto transform = tc.GetTransform();
				transform *= mesh->modelMatrix;

				g_RHI->BindParameter("SceneData", &g_Data->sceneDataCB);
				g_RHI->BindParameter("Transform", &transform, sizeof(glm::mat4));
				g_RHI->BindParameter("ObjectID", &entity, sizeof(uint32_t));

				for (auto& submesh : mesh->submeshes)
					g_RHI->DrawIndexed(submesh->indexCount, 1, submesh->indexStart);
			}
		}
		g_RHI->EndRenderPass(&g_Data->objectPickerPass);
	}

	void Renderer::DeferredRenderingPass()
	{
		auto entitiesToRender = g_Data->currentScene->GetAllEntitiesWith<MeshComponent>();

		// Deferred Base Pass
		g_RHI->BeginRenderPass(&g_Data->deferredBasePass);
		g_RHI->BindPipeline(&g_Data->pipelines["Deferred Base Pass"]);
		for (auto entity : entitiesToRender)
		{
			Entity e = { entity, g_Data->currentScene };
			MeshSource* ms = e.GetComponent<MeshComponent>().meshSource.Get();
			if (!ms->bHasMesh)
				continue;
			TransformComponent tc = e.GetComponent<TransformComponent>();

			g_RHI->BindVertexBuffer(&ms->meshVb);
			g_RHI->BindIndexBuffer(&ms->meshIb);
			for (auto& mesh : ms->meshes)
			{
				glm::mat4 transform = tc.GetTransform();
				transform *= mesh->modelMatrix;

				g_RHI->BindParameter("SceneData", &g_Data->sceneDataCB);
				g_RHI->BindParameter("Transform", &transform, sizeof(glm::mat4));

				for (auto& submesh : mesh->submeshes)
				{
					g_RHI->BindParameter("g_TextureDiffuse", &submesh->diffuseTexture);
					g_RHI->DrawIndexed(submesh->indexCount, 1, submesh->indexStart);
				}
			}
		}

		g_RHI->EndRenderPass(&g_Data->deferredBasePass);

		g_RHI->BeginRenderPass(&g_Data->deferredLightingPass);
		g_RHI->BindPipeline(&g_Data->pipelines["Deferred Lighting Pass"]);
		g_RHI->BindVertexBuffer(&g_Data->quadBuffer);
		g_RHI->BindParameter("SceneData", &g_Data->sceneDataCB);
		g_RHI->BindParameter("DirectionalLights", &g_Data->directionalLightsBuffer);
		g_RHI->BindParameter("PointLights", &g_Data->pointLightsBuffer);
		g_RHI->BindParameter("g_TextureBaseColor", &g_Data->deferredBasePass.colorAttachments[0]);
		g_RHI->BindParameter("g_TexturePosition", &g_Data->deferredBasePass.colorAttachments[1]);
		g_RHI->BindParameter("g_TextureNormal", &g_Data->deferredBasePass.colorAttachments[2]);
		g_RHI->Draw(6);

		// Skybox
		if (g_Data->bIsSkyboxEnabled)
		{
			g_RHI->BindPipeline(&g_Data->pipelines["Skybox"]);
			g_Data->skybox->Render(g_Data->projectionMatrix * glm::mat4(glm::mat3(g_Data->viewMatrix)));
		}

		g_RHI->EndRenderPass(&g_Data->deferredLightingPass);
	}

	void Renderer::ForwardRenderingPass()
	{
		auto entitiesToRender = g_Data->currentScene->GetAllEntitiesWith<MeshComponent>();

		// Forward Pass
		g_RHI->BeginRenderPass(&g_Data->forwardPass);
		g_RHI->BindPipeline(&g_Data->pipelines["Forward Rendering"]);
		for (auto entity : entitiesToRender)
		{
			Entity e = { entity, g_Data->currentScene };
			MeshSource* ms = e.GetComponent<MeshComponent>().meshSource.Get();
			if (!ms->bHasMesh)
				continue;
			TransformComponent tc = e.GetComponent<TransformComponent>();

			g_RHI->BindVertexBuffer(&ms->meshVb);
			g_RHI->BindIndexBuffer(&ms->meshIb);
			for (auto& mesh : ms->meshes)
			{
				auto transform = tc.GetTransform();
				transform *= mesh->modelMatrix;

				g_RHI->BindParameter("SceneData", &g_Data->sceneDataCB);
				g_RHI->BindParameter("Transform", &transform, sizeof(glm::mat4));
				g_RHI->BindParameter("DirectionalLights", &g_Data->directionalLightsBuffer);
				g_RHI->BindParameter("PointLights", &g_Data->pointLightsBuffer);

				for (auto& submesh : mesh->submeshes)
				{
					g_RHI->BindParameter("g_TextureDiffuse", &submesh->diffuseTexture);
					g_RHI->BindParameter("g_TextureEmissive", &submesh->emissiveTexture);
					g_RHI->DrawIndexed(submesh->indexCount, 1, submesh->indexStart);
				}
			}
		}

		// Skybox
		if (g_Data->bIsSkyboxEnabled)
		{
			g_RHI->BindPipeline(&g_Data->pipelines["Skybox"]);
			g_Data->skybox->Render(g_Data->projectionMatrix * glm::mat4(glm::mat3(g_Data->viewMatrix)));
		}
		g_RHI->EndRenderPass(&g_Data->forwardPass);
	}

	void Renderer::SceneCompositePass()
	{
		// Scene Composite
		g_RHI->BeginRenderPass(&g_Data->sceneComposite);
		g_RHI->BindPipeline(&g_Data->pipelines["Scene Composite"]);
		g_RHI->BindVertexBuffer(&g_Data->quadBuffer);
		if (g_Data->bIsDeferredEnabled)
		{
			if (g_Data->deferredLightingPass.colorAttachments.size() > 0)
				g_RHI->BindParameter("g_SceneTexture", &g_Data->deferredLightingPass.colorAttachments[0]);
		}
		else
		{
			if (g_Data->forwardPass.colorAttachments.size() > 0)
				g_RHI->BindParameter("g_SceneTexture", &g_Data->forwardPass.colorAttachments[0]);
		}
		GfxResult error = g_RHI->UpdateBufferData(&g_Data->sceneSettingsBuffer, &g_Data->sceneSettings);
		ensure(error == GfxResult::kNoError);
		g_RHI->BindParameter("SceneSettings", &g_Data->sceneSettingsBuffer);
		g_RHI->Draw(6);
		g_RHI->EndRenderPass(&g_Data->sceneComposite);
	}

	API Renderer::GetCurrentAPI()
	{
		return g_RHI->GetCurrentAPI();
	}

	void Renderer::PrepareScene()
	{
		if (g_Data->currentScene && g_Data->currentScene->IsSceneLoaded())
		{
			g_Data->skybox->Prepare();
			g_Data->currentScene->ExecutePreparations();
			return;
		}

		auto& sceneToLoad = g_Data->currentScene->GetScenePath();

		g_Data->currentScene->GetSelectedEntity().Invalidate();
		edelete g_Data->currentScene;
		g_Data->currentScene = enew Scene();

		if (!sceneToLoad.empty())
		{
			SceneSerializer serializer(g_Data->currentScene);
			serializer.Deserialize(sceneToLoad);

			auto entities = g_Data->currentScene->GetAllEntitiesWith<MeshComponent>();
			for (auto& entity : entities)
			{
				Entity e = { entity, g_Data->currentScene };
				e.GetComponent<MeshComponent>().LoadMeshSource();
			}

			g_Data->currentScene->SetScenePath(sceneToLoad);
		}

		g_Data->currentScene->SetSceneLoaded(true);
		Application::Get()->ChangeWindowTitle(g_Data->currentScene->GetName());
	}

	void Renderer::NewScene()
	{
		g_Data->currentScene->SetScenePath("");
		g_Data->currentScene->SetSceneLoaded(false);
	}

	void Renderer::OpenScene(const std::filesystem::path& path)
	{
		g_Data->currentScene->SetScenePath(path);
		g_Data->currentScene->SetSceneLoaded(false);
		g_Data->camera = Camera(g_Data->window->GetWidth(), g_Data->window->GetHeight()); // Reset camera
	}

	void Renderer::OpenSceneDialog()
	{
		std::string path = Application::Get()->OpenFileDialog("Eden Scene (.escene)\0*.escene\0");
		if (!path.empty())
			OpenScene(path);
	}

	void Renderer::SaveSceneAs()
	{
		std::filesystem::path path = Application::Get()->SaveFileDialog("Eden Scene (.escene)\0*.escene\0");
		if (!path.empty())
		{
			if (!path.has_extension())
				path += Utils::ExtensionToString(EdenExtension::kScene);

			SceneSerializer serializer(g_Data->currentScene);
			serializer.Serialize(path);
			g_Data->currentScene->SetScenePath(path);
			Application::Get()->ChangeWindowTitle(g_Data->currentScene->GetName());
		}
	}

	void Renderer::SaveScene()
	{
		if (g_Data->currentScene->GetScenePath().empty())
		{
			SaveSceneAs();
			return;
		}

		ED_LOG_INFO("Saved scene: {}", g_Data->currentScene->GetScenePath());
		SceneSerializer serializer(g_Data->currentScene);
		serializer.Serialize(g_Data->currentScene->GetScenePath());
	}

	Scene* Renderer::GetCurrentScene()
	{
		return g_Data->currentScene;
	}
	
	void Renderer::SetViewportSize(float x, float y)
	{
		if (Renderer::GetViewportSize() != glm::vec2(x, y))
		{
			g_Data->viewportSize = { x, y };
			g_Data->forwardPass.desc.width = (uint32_t)x;
			g_Data->forwardPass.desc.height = (uint32_t)y;
			g_Data->sceneComposite.desc.width = (uint32_t)x;
			g_Data->sceneComposite.desc.height = (uint32_t)y;
			g_Data->objectPickerPass.desc.width = (uint32_t)x;
			g_Data->objectPickerPass.desc.height = (uint32_t)y;
			g_Data->deferredBasePass.desc.width = (uint32_t)x;
			g_Data->deferredBasePass.desc.height = (uint32_t)y;
			g_Data->deferredLightingPass.desc.width = (uint32_t)x;
			g_Data->deferredLightingPass.desc.height = (uint32_t)y;
			g_Data->camera.SetViewportSize(g_Data->viewportSize);
		}
	}

	void Renderer::SetViewportSize(glm::vec2 size)
	{
		SetViewportSize(size.x, size.y);
	}

	glm::vec2 Renderer::GetViewportSize()
	{
		return g_Data->viewportSize;
	}

	void Renderer::SetCameraPosition(float x, float y)
	{
		g_Data->camera.SetViewportPosition({ x, y });
	}

	bool& Renderer::IsSkyboxEnabled()
	{
		return g_Data->bIsSkyboxEnabled;
	}

	bool& Renderer::IsDeferredRenderingEnabled()
	{
		return g_Data->bIsDeferredEnabled;
	}

	void Renderer::SetNewSkybox(const char* path)
	{
		g_Data->skybox->SetNewTexture(path);
	}

	RendererData::SceneSettings& Renderer::GetSceneSettings()
	{
		return g_Data->sceneSettings;
	}

	GPUTimer& Renderer::GetRenderTimer()
	{
		return g_Data->renderTimer;
	}

	glm::mat4 Renderer::GetViewMatrix()
	{
		return g_Data->viewMatrix;
	}

	glm::mat4 Renderer::GetProjectionMatrix()
	{
		return g_Data->projectionMatrix;
	}

	void Renderer::CreateRenderpass(RenderPass* renderPass, RenderPassDesc* desc)
	{
		GfxResult error = g_RHI->CreateRenderPass(renderPass, desc);
		ensure(error == GfxResult::kNoError);
	}

	void Renderer::BeginRenderPass(RenderPass* renderPass)
	{
		g_RHI->BeginRenderPass(renderPass);
	}

	void Renderer::EndRenderPass(RenderPass* renderPass)
	{
		g_RHI->EndRenderPass(renderPass);
	}

	void Renderer::SetSwapchainTarget(RenderPass* renderPass)
	{
		g_RHI->SetSwapchainTarget(renderPass);
	}

	void Renderer::EnableImGui()
	{
		g_RHI->EnableImGui();
	}

	void Renderer::ReadPixelFromTexture(uint32_t x, uint32_t y, Texture* texture, glm::vec4& pixel)
	{
		g_RHI->ReadPixelFromTexture(x, y, texture, pixel);
	}

	uint64_t Renderer::GetTextureID(Texture* texture)
	{
		return g_RHI->GetTextureID(texture);
	}

	void Renderer::ReloadPipeline(Pipeline* pipeline)
	{
		GfxResult error = g_RHI->ReloadPipeline(pipeline); 
		ensure(error == GfxResult::kNoError);
	}

	void Renderer::EnsureMsgResourceState(Texture* resource, ResourceState destResourceState)
	{
		g_RHI->EnsureMsgResourceState(resource, destResourceState);
	}

	void Renderer::CreateTexture(Texture* texture, std::string path, bool bGenerateMips)
	{
		GfxResult error = g_RHI->CreateTexture(texture, path, bGenerateMips);
		ensure(error == GfxResult::kNoError);
	}

	void Renderer::CreateTexture(Texture* texture, TextureDesc* desc)
	{
		GfxResult error = g_RHI->CreateTexture(texture, desc);
		ensure(error == GfxResult::kNoError);
	}

	void Renderer::CreateBuffer(Buffer* buffer, BufferDesc* desc, const void* initial_data)
	{
		GfxResult error = g_RHI->CreateBuffer(buffer, desc, initial_data);
		ensure(error == GfxResult::kNoError);
	}

	void Renderer::SetName(Resource* child, std::string& name)
	{
		g_RHI->SetName(child, name);
	}

	void Renderer::BindPipeline(Pipeline* pipeline)
	{
		g_RHI->BindPipeline(pipeline);
	}

	void Renderer::BindVertexBuffer(Buffer* vertexBuffer)
	{
		g_RHI->BindVertexBuffer(vertexBuffer);
	}

	void Renderer::BindIndexBuffer(Buffer* indexBuffer)
	{
		g_RHI->BindIndexBuffer(indexBuffer);
	}

	void Renderer::BindParameter(const std::string& parameterName, Buffer* buffer)
	{
		g_RHI->BindParameter(parameterName, buffer);
	}

	void Renderer::BindParameter(const std::string& parameterName, Texture* texture, TextureUsage usage /*= kReadOnly*/)
	{
		g_RHI->BindParameter(parameterName, texture, usage);
	}

	void Renderer::BindParameter(const std::string& parameterName, void* data, size_t size)
	{
		g_RHI->BindParameter(parameterName, data, size);
	}

	void Renderer::Draw(uint32_t vertexCount, uint32_t instanceCount /*= 1*/, uint32_t startVertexLocation /*= 0*/, uint32_t startInstanceLocation /*= 0*/)
	{
		g_RHI->Draw(vertexCount, instanceCount, startVertexLocation, startInstanceLocation);
	}

	void Renderer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount /*= 1*/, uint32_t startIndexLocation /*= 0*/, uint32_t baseVertexLocation /*= 0*/, uint32_t startInstanceLocation /*= 0*/)
	{
		g_RHI->DrawIndexed(indexCount, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
	}

	void Renderer::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
	{
		g_RHI->Dispatch(groupCountX, groupCountY, groupCountZ);
	}

	void Renderer::GetPixel(uint32_t x, uint32_t y, glm::vec4& pixel)
	{
		g_RHI->ReadPixelFromTexture(x, y, &g_Data->objectPickerPass.colorAttachments[0], pixel);
	}

	Texture& Renderer::GetFinalImage()
	{
		return g_Data->sceneComposite.colorAttachments[0];
	}

	std::unordered_map<const char*, Pipeline>& Renderer::GetPipelines()
	{
		return g_Data->pipelines;
	}

}