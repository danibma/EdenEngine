#include "Renderer.h"

#include "RHI/D3D12/D3D12DynamicRHI.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Core/Application.h"
#include "Core/CommandLine.h"

namespace Eden
{
	// Renderer Data
	RendererData* Renderer::m_Data		   = nullptr;
	bool Renderer::m_IsRendererInitialized = false;

	void Renderer::Init(Window* window)
	{
		RHICreate(window);

		m_Data = enew RendererData();
		m_Data->viewportSize = { static_cast<float>(window->GetWidth()), static_cast<float>(window->GetHeight()) };

		m_Data->window = window;
		m_Data->camera = Camera(window->GetWidth(), window->GetHeight());

		m_Data->viewMatrix = glm::lookAtLH(m_Data->camera.position, m_Data->camera.position + m_Data->camera.front, m_Data->camera.up);
		m_Data->projectionMatrix = glm::perspectiveFovLH_ZO(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 200.0f);
		m_Data->sceneData.view = m_Data->viewMatrix;
		m_Data->sceneData.viewProjection = m_Data->projectionMatrix * m_Data->viewMatrix;
		m_Data->sceneData.viewPosition = glm::vec4(m_Data->camera.position, 1.0f);

		BufferDesc sceneDataDesc = {};
		sceneDataDesc.elementCount = 1;
		sceneDataDesc.stride = sizeof(RendererData::SceneData);
		m_Data->sceneDataCB = RHICreateBuffer(&sceneDataDesc, &m_Data->sceneData);

		m_Data->currentScene = enew Scene();
		std::string defaultScene = "assets/scenes/flying_world.escene";
		OpenScene(defaultScene);

		m_Data->skybox = MakeShared<Skybox>("assets/skyboxes/studio_garden.hdr");

		// Lights
		BufferDesc dl_desc = {};
		dl_desc.elementCount = m_Data->MAX_DIRECTIONAL_LIGHTS;
		dl_desc.stride = sizeof(DirectionalLightComponent);
		dl_desc.usage = BufferDesc::Storage;
		m_Data->directionalLightsBuffer = RHICreateBuffer(&dl_desc, nullptr);

		BufferDesc pl_desc = {};
		pl_desc.elementCount = m_Data->MAX_POINT_LIGHTS;
		pl_desc.stride = sizeof(PointLightComponent);
		pl_desc.usage = BufferDesc::Storage;
		m_Data->pointLightsBuffer = RHICreateBuffer(&pl_desc, nullptr);

		UpdateDirectionalLights();
		UpdatePointLights();

		// Object Picker
		{
			RenderPassDesc desc = {};
			desc.debugName = "ObjectPicker";
			desc.attachmentsFormats = { Format::kRGBA32_FLOAT, Format::Depth };
			desc.width = static_cast<uint32_t>(m_Data->viewportSize.x);
			desc.height = static_cast<uint32_t>(m_Data->viewportSize.y);
			desc.clearColor = glm::vec4(-1);
			m_Data->objectPickerPass = RHICreateRenderPass(&desc);

			PipelineDesc pipelineDesc = {};
			pipelineDesc.programName  = "ObjectPicker";
			pipelineDesc.renderPass   = m_Data->objectPickerPass;
			m_Data->pipelines["Object Picker"] = RHICreatePipeline(&pipelineDesc);
		}

		// Forward Pass
		{
			RenderPassDesc desc = {};
			desc.debugName = "ForwardPass";
			desc.attachmentsFormats = { Format::kRGBA32_FLOAT, Format::Depth };
			desc.width = static_cast<uint32_t>(m_Data->viewportSize.x);
			desc.height = static_cast<uint32_t>(m_Data->viewportSize.y);
			m_Data->forwardPass = RHICreateRenderPass(&desc);

			PipelineDesc forwardDesc    = {};
			forwardDesc.bEnableBlending = true;
			forwardDesc.programName     = "ForwardPass";
			forwardDesc.renderPass      = m_Data->forwardPass;
			m_Data->pipelines["Forward Rendering"] = RHICreatePipeline(&forwardDesc);

			PipelineDesc skyboxDesc = {};
			skyboxDesc.cull_mode    = CullMode::kNone;
			skyboxDesc.depthFunc    = ComparisonFunc::kLessEqual;
			skyboxDesc.minDepth     = 1.0f;
			skyboxDesc.programName  = "Skybox";
			skyboxDesc.renderPass   = m_Data->forwardPass;
			m_Data->pipelines["Skybox"] = RHICreatePipeline(&skyboxDesc);
		}

		// Deferred Pass
		{
			RenderPassDesc desc = {};
			desc.debugName = "DeferredBasePass";
			desc.attachmentsFormats = { Format::kRGBA32_FLOAT, Format::kRGBA32_FLOAT, Format::kRGBA32_FLOAT, Format::kRGBA32_FLOAT, Format::kRGBA32_FLOAT, Format::Depth };
			desc.width = static_cast<uint32_t>(m_Data->viewportSize.x);
			desc.height = static_cast<uint32_t>(m_Data->viewportSize.y);
			desc.clearColor = { 0, 0, 0, 0 };
			m_Data->deferredBasePass = RHICreateRenderPass(&desc);

			PipelineDesc deferredBaseDesc = {};
			deferredBaseDesc.bEnableBlending = true;
			deferredBaseDesc.programName = "DeferredBasePass";
			deferredBaseDesc.renderPass = m_Data->deferredBasePass;
			m_Data->pipelines["Deferred Base Pass"] = RHICreatePipeline(&deferredBaseDesc);

			RenderPassDesc lightingDesc = {};
			desc.debugName = "DeferredLightingPass";
			desc.attachmentsFormats = { Format::kRGBA32_FLOAT, Format::Depth };
			desc.width = static_cast<uint32_t>(m_Data->viewportSize.x);
			desc.height = static_cast<uint32_t>(m_Data->viewportSize.y);
			m_Data->deferredLightingPass = RHICreateRenderPass(&desc);

			PipelineDesc deferredLightingPass = {};
			deferredLightingPass.cull_mode = CullMode::kNone;
			deferredLightingPass.bEnableBlending = false;
			deferredLightingPass.programName = "DeferredLightingPass";
			deferredLightingPass.renderPass = m_Data->deferredLightingPass;
			m_Data->pipelines["Deferred Lighting Pass"] = RHICreatePipeline(&deferredLightingPass);
		}

		// Scene Composite
		{
			RenderPassDesc desc = {};
			desc.debugName = "SceneComposite";
			desc.attachmentsFormats = { Format::kRGBA32_FLOAT, Format::Depth };
#ifndef WITH_EDITOR
			desc.bIsSwapchainTarget = true;
#endif
			desc.width = static_cast<uint32_t>(m_Data->viewportSize.x);
			desc.height = static_cast<uint32_t>(m_Data->viewportSize.y);
			m_Data->sceneComposite = RHICreateRenderPass(&desc);
#ifndef WITH_EDITOR
			RHISetSwapchainTarget(m_Data->sceneComposite);
#endif

			PipelineDesc sceneCompositeDesc = {};
			sceneCompositeDesc.cull_mode = CullMode::kNone;
			sceneCompositeDesc.bEnableBlending = false;
			sceneCompositeDesc.programName = "SceneComposite";
			sceneCompositeDesc.renderPass = m_Data->sceneComposite;
			m_Data->pipelines["Scene Composite"] = RHICreatePipeline(&sceneCompositeDesc);
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
		m_Data->quadBuffer = RHICreateBuffer(&quadDesc, quadVertices);

		BufferDesc sceneSettingsDesc = {};
		sceneSettingsDesc.elementCount = 1;
		sceneSettingsDesc.stride = sizeof(RendererData::SceneSettings);
		sceneSettingsDesc.usage = BufferDesc::Uniform;
		m_Data->sceneSettingsBuffer = RHICreateBuffer(&sceneSettingsDesc, &m_Data->sceneSettings);

		m_Data->renderTimer = RHICreateGPUTimer();

		ED_LOG_INFO("Renderer has been initialized!");
		m_IsRendererInitialized = true;
	}

	void Renderer::BeginRender()
	{
		PrepareScene();

		// Update camera and scene data
		m_Data->camera.Update(Application::Get()->GetDeltaTime());
		m_Data->viewMatrix = glm::lookAtLH(m_Data->camera.position, m_Data->camera.position + m_Data->camera.front, m_Data->camera.up);
		m_Data->projectionMatrix = glm::perspectiveFovLH(glm::radians(70.0f), m_Data->viewportSize.x, m_Data->viewportSize.y, 0.1f, 200.0f);
		m_Data->sceneData.view = m_Data->viewMatrix;
		m_Data->sceneData.viewProjection = m_Data->projectionMatrix * m_Data->viewMatrix;
		m_Data->sceneData.viewPosition = glm::vec4(m_Data->camera.position, 1.0f);
		RHIUpdateBufferData(m_Data->sceneDataCB, &m_Data->sceneData);

		UpdateDirectionalLights();
		UpdatePointLights();

		RHIBeginRender();
	}

	void Renderer::Render()
	{
		RHIBeginGPUTimer(m_Data->renderTimer);

#if WITH_EDITOR
		ObjectPickerPass();
#endif
		if (m_Data->bIsDeferredEnabled)
			DeferredRenderingPass();
		else
			ForwardRenderingPass();
		SceneCompositePass();
	}

	void Renderer::EndRender()
	{
		RHIEndGPUTimer(m_Data->renderTimer);
		RHIEndRender();
		RHIRender();
	}

	void Renderer::Shutdown()
	{
		edelete m_Data->currentScene;
		edelete m_Data;

		RHIShutdown();
	}

	bool Renderer::IsInitialized()
	{
		return m_IsRendererInitialized;
	}

	void Renderer::UpdatePointLights()
	{
		PointLightComponent emptyPl;
		emptyPl.color = glm::vec4(0);

		std::array<PointLightComponent, m_Data->MAX_POINT_LIGHTS> pointLightComponents;
		pointLightComponents.fill(emptyPl);

		auto pointLights = m_Data->currentScene->GetAllEntitiesWith<PointLightComponent>();
		for (int i = 0; i < pointLights.size(); ++i)
		{
			Entity e = { pointLights[i], m_Data->currentScene };
			PointLightComponent& pl = e.GetComponent<PointLightComponent>();
			pointLightComponents[i] = pl;
		}

		const void* data = pointLightComponents.data();
		RHIUpdateBufferData(m_Data->pointLightsBuffer, data);
	}

	void Renderer::UpdateDirectionalLights()
	{
		DirectionalLightComponent emptyDl;
		emptyDl.intensity = 0;

		std::array<DirectionalLightComponent, m_Data->MAX_DIRECTIONAL_LIGHTS> directional_lightComponents;
		directional_lightComponents.fill(emptyDl);

		auto directional_lights = m_Data->currentScene->GetAllEntitiesWith<DirectionalLightComponent>();
		for (int i = 0; i < directional_lights.size(); ++i)
		{
			Entity e = { directional_lights[i], m_Data->currentScene };
			DirectionalLightComponent& pl = e.GetComponent<DirectionalLightComponent>();
			directional_lightComponents[i] = pl;
		}

		//if (directional_lights.size() > 0)
		//	m_DirectionalLight = { directional_lights[0], m_CurrentScene };

		const void* data = directional_lightComponents.data();
		RHIUpdateBufferData(m_Data->directionalLightsBuffer, data);
	}
	
	void Renderer::ObjectPickerPass()
	{
		auto entitiesToRender = m_Data->currentScene->GetAllEntitiesWith<MeshComponent>();

		RHIBeginRenderPass(m_Data->objectPickerPass);
		RHIBindPipeline(m_Data->pipelines["Object Picker"]);
		for (auto entity : entitiesToRender)
		{
			Entity e = { entity, m_Data->currentScene };
			MeshSource* ms = e.GetComponent<MeshComponent>().meshSource.Get();
			if (!ms->bHasMesh)
				continue;
			TransformComponent tc = e.GetComponent<TransformComponent>();

			RHIBindVertexBuffer(ms->meshVb);
			RHIBindIndexBuffer(ms->meshIb);
			for (auto& mesh : ms->meshes)
			{
				auto transform = tc.GetTransform();
				transform *= mesh->modelMatrix;

				RHIBindParameter("SceneData", m_Data->sceneDataCB);
				RHIBindParameter("Transform", &transform, sizeof(glm::mat4));
				RHIBindParameter("ObjectID", &entity, sizeof(uint32_t));

				for (auto& submesh : mesh->submeshes)
					RHIDrawIndexed(submesh->indexCount, 1, submesh->indexStart);
			}
		}
		RHIEndRenderPass(m_Data->objectPickerPass);
	}

	void Renderer::DeferredRenderingPass()
	{
		auto entitiesToRender = m_Data->currentScene->GetAllEntitiesWith<MeshComponent>();

		// Deferred Base Pass
		RHIBeginRenderPass(m_Data->deferredBasePass);
		RHIBindPipeline(m_Data->pipelines["Deferred Base Pass"]);
		for (auto entity : entitiesToRender)
		{
			Entity e = { entity, m_Data->currentScene };
			MeshSource* ms = e.GetComponent<MeshComponent>().meshSource.Get();
			if (!ms->bHasMesh)
				continue;
			TransformComponent tc = e.GetComponent<TransformComponent>();

			RHIBindVertexBuffer(ms->meshVb);
			RHIBindIndexBuffer(ms->meshIb);
			for (auto& mesh : ms->meshes)
			{
				glm::mat4 transform = tc.GetTransform();
				transform *= mesh->modelMatrix;

				RHIBindParameter("SceneData", m_Data->sceneDataCB);
				RHIBindParameter("Transform", &transform, sizeof(glm::mat4));

				for (auto& submesh : mesh->submeshes)
				{
					RHIBindParameter("g_AlbedoMap", submesh->material.albedoMap);
					RHIBindParameter("g_NormalMap", submesh->material.normalMap);
					RHIBindParameter("g_AOMap", submesh->material.AOMap);
					RHIBindParameter("g_EmissiveMap", submesh->material.emissiveMap);
					RHIBindParameter("g_MetallicRoughnessMap", submesh->material.metallicRoughnessMap);
					RHIDrawIndexed(submesh->indexCount, 1, submesh->indexStart);
				}
			}
		}

		RHIEndRenderPass(m_Data->deferredBasePass);

		RHIBeginRenderPass(m_Data->deferredLightingPass);
		RHIBindPipeline(m_Data->pipelines["Deferred Lighting Pass"]);
		RHIBindVertexBuffer(m_Data->quadBuffer);
		RHIBindParameter("SceneData", m_Data->sceneDataCB);
		RHIBindParameter("DirectionalLights", m_Data->directionalLightsBuffer);
		RHIBindParameter("PointLights", m_Data->pointLightsBuffer);
		RHIBindParameter("g_TextureBaseColor", m_Data->deferredBasePass->colorAttachments[0]);
		RHIBindParameter("g_TexturePosition", m_Data->deferredBasePass->colorAttachments[1]);
		RHIBindParameter("g_TextureNormal", m_Data->deferredBasePass->colorAttachments[2]);
		RHIBindParameter("g_TextureMetallicRoughnessAO", m_Data->deferredBasePass->colorAttachments[3]);
		RHIBindParameter("g_TextureNormalMap", m_Data->deferredBasePass->colorAttachments[4]);
		RHIDraw(6);

		// Skybox
		if (m_Data->bIsSkyboxEnabled)
		{
			RHIBindPipeline(m_Data->pipelines["Skybox"]);
			m_Data->skybox->Render(m_Data->projectionMatrix * glm::mat4(glm::mat3(m_Data->viewMatrix)));
		}

		RHIEndRenderPass(m_Data->deferredLightingPass);
	}

	void Renderer::ForwardRenderingPass()
	{
		auto entitiesToRender = m_Data->currentScene->GetAllEntitiesWith<MeshComponent>();

		// Forward Pass
		RHIBeginRenderPass(m_Data->forwardPass);
		RHIBindPipeline(m_Data->pipelines["Forward Rendering"]);
		for (auto entity : entitiesToRender)
		{
			Entity e = { entity, m_Data->currentScene };
			MeshSource* ms = e.GetComponent<MeshComponent>().meshSource.Get();
			if (!ms->bHasMesh)
				continue;
			TransformComponent tc = e.GetComponent<TransformComponent>();

			RHIBindVertexBuffer(ms->meshVb);
			RHIBindIndexBuffer(ms->meshIb);
			for (auto& mesh : ms->meshes)
			{
				auto transform = tc.GetTransform();
				transform *= mesh->modelMatrix;

				RHIBindParameter("SceneData", m_Data->sceneDataCB);
				RHIBindParameter("Transform", &transform, sizeof(glm::mat4));
				RHIBindParameter("DirectionalLights", m_Data->directionalLightsBuffer);
				RHIBindParameter("PointLights", m_Data->pointLightsBuffer);

				for (auto& submesh : mesh->submeshes)
				{
					RHIBindParameter("g_AlbedoMap", submesh->material.albedoMap);
					RHIBindParameter("g_NormalMap", submesh->material.normalMap);
					RHIBindParameter("g_AOMap", submesh->material.AOMap);
					RHIBindParameter("g_EmissiveMap", submesh->material.emissiveMap);
					RHIBindParameter("g_MetallicRoughnessMap", submesh->material.metallicRoughnessMap);
					RHIDrawIndexed(submesh->indexCount, 1, submesh->indexStart);
				}
			}
		}

		// Skybox
		if (m_Data->bIsSkyboxEnabled)
		{
			RHIBindPipeline(m_Data->pipelines["Skybox"]);
			m_Data->skybox->Render(m_Data->projectionMatrix * glm::mat4(glm::mat3(m_Data->viewMatrix)));
		}
		RHIEndRenderPass(m_Data->forwardPass);
	}

	void Renderer::SceneCompositePass()
	{
		// Scene Composite
		RHIBeginRenderPass(m_Data->sceneComposite);
		RHIBindPipeline(m_Data->pipelines["Scene Composite"]);
		RHIBindVertexBuffer(m_Data->quadBuffer);
		if (m_Data->bIsDeferredEnabled)
		{
			if (m_Data->deferredLightingPass->colorAttachments.size() > 0)
				RHIBindParameter("g_SceneTexture", m_Data->deferredLightingPass->colorAttachments[0]);
		}
		else
		{
			if (m_Data->forwardPass->colorAttachments.size() > 0)
				RHIBindParameter("g_SceneTexture", m_Data->forwardPass->colorAttachments[0]);
		}
		RHIUpdateBufferData(m_Data->sceneSettingsBuffer, &m_Data->sceneSettings);
		RHIBindParameter("SceneSettings", m_Data->sceneSettingsBuffer);
		RHIDraw(6);
		RHIEndRenderPass(m_Data->sceneComposite);
	}

	void Renderer::PrepareScene()
	{
		if (m_Data->currentScene && m_Data->currentScene->IsSceneLoaded())
		{
			m_Data->skybox->Prepare();
			m_Data->currentScene->ExecutePreparations();
			return;
		}

		auto& sceneToLoad = m_Data->currentScene->GetScenePath();

		m_Data->currentScene->GetSelectedEntity().Invalidate();
		edelete m_Data->currentScene;
		m_Data->currentScene = enew Scene();

		if (!sceneToLoad.empty())
		{
			SceneSerializer serializer(m_Data->currentScene);
			serializer.Deserialize(sceneToLoad);

			auto entities = m_Data->currentScene->GetAllEntitiesWith<MeshComponent>();
			for (auto& entity : entities)
			{
				Entity e = { entity, m_Data->currentScene };
				e.GetComponent<MeshComponent>().LoadMeshSource();
			}

			m_Data->currentScene->SetScenePath(sceneToLoad);
		}

		m_Data->currentScene->SetSceneLoaded(true);
		Application::Get()->ChangeWindowTitle(m_Data->currentScene->GetName());
	}

	void Renderer::NewScene()
	{
		m_Data->currentScene->SetScenePath("");
		m_Data->currentScene->SetSceneLoaded(false);
	}

	void Renderer::OpenScene(const std::filesystem::path& path)
	{
		m_Data->currentScene->SetScenePath(path);
		m_Data->currentScene->SetSceneLoaded(false);
		m_Data->camera = Camera(m_Data->window->GetWidth(), m_Data->window->GetHeight()); // Reset camera
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

			SceneSerializer serializer(m_Data->currentScene);
			serializer.Serialize(path);
			m_Data->currentScene->SetScenePath(path);
			Application::Get()->ChangeWindowTitle(m_Data->currentScene->GetName());
		}
	}

	void Renderer::SaveScene()
	{
		if (m_Data->currentScene->GetScenePath().empty())
		{
			SaveSceneAs();
			return;
		}

		ED_LOG_INFO("Saved scene: {}", m_Data->currentScene->GetScenePath());
		SceneSerializer serializer(m_Data->currentScene);
		serializer.Serialize(m_Data->currentScene->GetScenePath());
	}

	Scene* Renderer::GetCurrentScene()
	{
		return m_Data->currentScene;
	}
	
	void Renderer::SetViewportSize(float x, float y)
	{
		if (Renderer::GetViewportSize() != glm::vec2(x, y))
		{
			m_Data->viewportSize = { x, y };
			m_Data->forwardPass->desc.width = (uint32_t)x;
			m_Data->forwardPass->desc.height = (uint32_t)y;
			m_Data->sceneComposite->desc.width = (uint32_t)x;
			m_Data->sceneComposite->desc.height = (uint32_t)y;
			m_Data->objectPickerPass->desc.width = (uint32_t)x;
			m_Data->objectPickerPass->desc.height = (uint32_t)y;
			m_Data->deferredBasePass->desc.width = (uint32_t)x;
			m_Data->deferredBasePass->desc.height = (uint32_t)y;
			m_Data->deferredLightingPass->desc.width = (uint32_t)x;
			m_Data->deferredLightingPass->desc.height = (uint32_t)y;
			m_Data->camera.SetViewportSize(m_Data->viewportSize);
		}
	}

	void Renderer::SetViewportSize(glm::vec2 size)
	{
		SetViewportSize(size.x, size.y);
	}

	glm::vec2 Renderer::GetViewportSize()
	{
		return m_Data->viewportSize;
	}

	void Renderer::SetCameraPosition(float x, float y)
	{
		m_Data->camera.SetViewportPosition({ x, y });
	}

	bool& Renderer::IsSkyboxEnabled()
	{
		return m_Data->bIsSkyboxEnabled;
	}

	bool& Renderer::IsDeferredRenderingEnabled()
	{
		return m_Data->bIsDeferredEnabled;
	}

	void Renderer::SetNewSkybox(const char* path)
	{
		m_Data->skybox->SetNewTexture(path);
	}

	RendererData::SceneSettings& Renderer::GetSceneSettings()
	{
		return m_Data->sceneSettings;
	}

	GPUTimerRef Renderer::GetRenderTimer()
	{
		return m_Data->renderTimer;
	}

	glm::mat4 Renderer::GetViewMatrix()
	{
		return m_Data->viewMatrix;
	}

	glm::mat4 Renderer::GetProjectionMatrix()
	{
		return m_Data->projectionMatrix;
	}
	
	void Renderer::GetPixel(uint32_t x, uint32_t y, glm::vec4& pixel)
	{
		RHIReadPixelFromTexture(x, y, m_Data->objectPickerPass->colorAttachments[0], pixel);
	}

	TextureRef Renderer::GetFinalImage()
	{
		return m_Data->sceneComposite->colorAttachments[0];
	}

	std::unordered_map<const char*, PipelineRef>& Renderer::GetPipelines()
	{
		return m_Data->pipelines;
	}

}