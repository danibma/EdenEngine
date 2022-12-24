#include "Core/Window.h"
#include "Core/Memory.h"
#include "Core/Input.h"
#include "Core/Log.h"
#include "Core/Assertions.h"
#include "Core/Application.h"
#include "UI/UI.h"
#include "Core/Camera.h"
#include "Profiling/Profiler.h"
#include "Graphics/D3D12/D3D12RHI.h"
#include "Graphics/Skybox.h"
#include "Scene/MeshSource.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Scene/SceneSerializer.h"
#include "Math/Math.h"
#include "Editor/ContentBrowserPanel.h"
#include "Editor/SceneHierarchy.h"
#include "Utilities/Utils.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <vector>

#define WITH_EDITOR 1

using namespace Eden;

class EdenApplication : public Application
{
	//==================
	// Scene Data
	//==================
	struct SceneData
	{
		glm::mat4 view;
		glm::mat4 viewProjection;
		glm::vec4 viewPosition;
	};

	//==================
	// Compute Shader CB
	//==================
	struct ComputeData
	{
		alignas(16) glm::vec2 resolution;
		float time;
	} m_ComputeData;
	std::shared_ptr<Buffer> m_ComputeBuffer;
	std::shared_ptr<Texture> m_OutputTexture;

	// TODO: Put this into the renderer and add a validation when adding directional lights on the editor
	static constexpr int MAX_DIRECTIONAL_LIGHTS = 2;
	std::shared_ptr<Buffer> m_DirectionalLightsBuffer;
	static constexpr int MAX_POINT_LIGHTS = 32;
	std::shared_ptr<Buffer> m_PointLightsBuffer;

	std::unordered_map<const char*, std::shared_ptr<Pipeline>> m_Pipelines;

	glm::mat4 m_ViewMatrix;
	glm::mat4 m_ProjectionMatrix;
	SceneData m_SceneData;
	std::shared_ptr<Buffer> m_SceneDataCB;
	Camera m_Camera;

	Entity m_Helmet;
	Entity m_BasicMesh;
	Entity m_FloorMesh;
	Entity m_DirectionalLight;

	// Skybox
	std::shared_ptr<Skybox> m_Skybox;
	bool m_bIsSkyboxEnabled = true;

	// UI
	bool m_bOpenStatisticsWindow = true;
	bool m_bOpenSceneProperties = true;
	bool m_bOpenPipelinesPanel = true;
	bool m_bOpenMemoryPanel = true;
	int m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
	glm::vec2 m_ViewportSize;
	glm::vec2 m_ViewportPos;
	bool m_bIsViewportFocused = false;
	bool m_bIsViewportHovered = false;
	std::unique_ptr<ContentBrowserPanel> m_ContentBrowserPanel;
	std::unique_ptr<SceneHierarchy> m_SceneHierarchy;

	// Scene
	Scene* m_CurrentScene = nullptr;

	// Rendering
	std::shared_ptr<RenderPass> m_GBuffer;
	std::shared_ptr<RenderPass> m_ImGuiPass;
	std::shared_ptr<RenderPass> m_SceneComposite;
	std::shared_ptr<Buffer> m_QuadBuffer;
	std::shared_ptr<GPUTimer> m_RenderTimer; // TODO: In the future make a vector of this timers inside the renderer class
	std::shared_ptr<GPUTimer> m_ComputeTimer;
	std::shared_ptr<RenderPass> m_ObjectPickerPass; // Editor Only

	struct SceneSettings
	{
		float exposure = 0.6f;
	} m_SceneSettings;
	std::shared_ptr<Buffer> m_SceneSettingsBuffer;

public:
	EdenApplication() = default;

	void OnInit() override
	{
		m_ViewportSize = { window->GetWidth(), window->GetHeight() };
		m_ViewportPos = { 0, 0 };

		m_Camera = Camera(window->GetWidth(), window->GetHeight());

		m_ViewMatrix = glm::lookAtLH(m_Camera.position, m_Camera.position + m_Camera.front, m_Camera.up);
		m_ProjectionMatrix = glm::perspectiveFovLH_ZO(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 200.0f);
		m_SceneData.view = m_ViewMatrix;
		m_SceneData.viewProjection = m_ProjectionMatrix * m_ViewMatrix;
		m_SceneData.viewPosition = glm::vec4(m_Camera.position, 1.0f);

		BufferDesc sceneDataDesc = {};
		sceneDataDesc.elementCount = 1;
		sceneDataDesc.stride = sizeof(SceneData);
		m_SceneDataCB = rhi->CreateBuffer(&sceneDataDesc, &m_SceneData);

		m_CurrentScene = enew Scene();
		std::string defaultScene = "assets/scenes/cube.escene";
		OpenScene(defaultScene);

		m_Skybox = std::make_shared<Skybox>(rhi, "assets/skyboxes/san_giuseppe_bridge.hdr");

		// Lights
		BufferDesc dl_desc = {};
		dl_desc.elementCount = MAX_DIRECTIONAL_LIGHTS;
		dl_desc.stride = sizeof(DirectionalLightComponent);
		dl_desc.usage = BufferDesc::Storage;
		m_DirectionalLightsBuffer = rhi->CreateBuffer(&dl_desc, nullptr);
		UpdateDirectionalLights();

		BufferDesc pl_desc = {};
		pl_desc.elementCount = MAX_POINT_LIGHTS;
		pl_desc.stride = sizeof(PointLightComponent);
		pl_desc.usage = BufferDesc::Storage;
		m_PointLightsBuffer = rhi->CreateBuffer(&pl_desc, nullptr);
		UpdatePointLights();


		// Object Picker
		{
			RenderPassDesc desc = {};
			desc.debugName = "ObjectPicker";
			desc.attachmentsFormats = { Format::RGBA32_FLOAT, Format::Depth };
			desc.width = static_cast<uint32_t>(m_ViewportSize.x);
			desc.height = static_cast<uint32_t>(m_ViewportSize.y);
			desc.clearColor = glm::vec4(-1);
			m_ObjectPickerPass = rhi->CreateRenderPass(&desc);

			PipelineDesc pipelineDesc = {};
			pipelineDesc.programName = "ObjectPicker";
			pipelineDesc.renderPass = m_ObjectPickerPass;
			m_Pipelines["Object Picker"] = rhi->CreatePipeline(&pipelineDesc);
		}

		// GBuffer
		{
			RenderPassDesc desc = {};
			desc.debugName = "GBuffer";
			desc.attachmentsFormats = { Format::RGBA32_FLOAT, Format::Depth };
			desc.width = static_cast<uint32_t>(m_ViewportSize.x);
			desc.height = static_cast<uint32_t>(m_ViewportSize.y);
			m_GBuffer = rhi->CreateRenderPass(&desc);

			PipelineDesc phongLightingDesc = {};
			phongLightingDesc.bEnableBlending = true;
			phongLightingDesc.programName = "PhongLighting";
			phongLightingDesc.renderPass = m_GBuffer;
			m_Pipelines["Phong Lighting"] = rhi->CreatePipeline(&phongLightingDesc);

			PipelineDesc skyboxDesc = {};
			skyboxDesc.cull_mode = CullMode::NONE;
			skyboxDesc.depthFunc = ComparisonFunc::LESS_EQUAL;
			skyboxDesc.minDepth = 1.0f;
			skyboxDesc.programName = "Skybox";
			skyboxDesc.renderPass = m_GBuffer;
			m_Pipelines["Skybox"] = rhi->CreatePipeline(&skyboxDesc);
		}

	#if WITH_EDITOR
		// Scene Composite
		{
			RenderPassDesc desc = {};
			desc.debugName = "SceneComposite";
			desc.attachmentsFormats = { Format::RGBA32_FLOAT, Format::Depth };
			desc.width = static_cast<uint32_t>(m_ViewportSize.x);
			desc.height = static_cast<uint32_t>(m_ViewportSize.y);
			m_SceneComposite = rhi->CreateRenderPass(&desc);

			PipelineDesc sceneCompositeDesc = {};
			sceneCompositeDesc.programName = "SceneComposite";
			sceneCompositeDesc.renderPass = m_SceneComposite;
			m_Pipelines["Scene Composite"] = rhi->CreatePipeline(&sceneCompositeDesc);
		}

		{
			RenderPassDesc desc = {};
			desc.debugName = "ImGuiPass";
			desc.attachmentsFormats = { Format::RGBA32_FLOAT, Format::Depth };
			desc.bIsSwapchainTarget = true;
			desc.bIsImguiPass = true;
			desc.width = static_cast<uint32_t>(m_ViewportSize.x);
			desc.height = static_cast<uint32_t>(m_ViewportSize.y);
			m_ImGuiPass = rhi->CreateRenderPass(&desc);
			rhi->SetSwapchainTarget(m_ImGuiPass);
			rhi->EnableImGui();
		}

		m_ContentBrowserPanel = std::make_unique<ContentBrowserPanel>(rhi);
		m_SceneHierarchy = std::make_unique<SceneHierarchy>(rhi, m_CurrentScene);
	#else
		// Scene Composite
		{
			RenderPassDesc desc = {};
			desc.debugName = "SceneComposite";
			desc.attachmentsFormats = { Format::RGBA32_FLOAT, Format::Depth };
			desc.bIsSwapchainTarget = true;
			desc.width = static_cast<uint32_t>(m_ViewportSize.x);
			desc.height = static_cast<uint32_t>(m_ViewportSize.y);
			m_SceneComposite = rhi->CreateRenderPass(&desc);
			rhi->SetSwapchainTarget(m_SceneComposite);

			PipelineDesc sceneCompositeDesc = {};
			sceneCompositeDesc.programName = "SceneComposite";
			sceneCompositeDesc.renderPass = m_SceneComposite;
			m_Pipelines["Scene Composite"] = rhi->CreatePipeline(&sceneCompositeDesc);
		}
	#endif

		PipelineDesc cubesTestDesc = {};
		cubesTestDesc.programName = "CubesCS";
		cubesTestDesc.type = Compute;
		m_Pipelines["Cubes Test"] = rhi->CreatePipeline(&cubesTestDesc);

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
		m_QuadBuffer = rhi->CreateBuffer(&quadDesc, quadVertices);

		BufferDesc sceneSettingsDesc = {};
		sceneSettingsDesc.elementCount = 1;
		sceneSettingsDesc.stride = sizeof(SceneSettings);
		sceneSettingsDesc.usage = BufferDesc::Uniform;
		m_SceneSettingsBuffer = rhi->CreateBuffer(&sceneSettingsDesc, &m_SceneSettings);

		// Compute shader
		m_ComputeData.resolution = glm::vec2(m_ViewportSize.x, m_ViewportSize.y);
		BufferDesc computeDataDesc = {};
		computeDataDesc.elementCount = 1;
		computeDataDesc.stride = sizeof(SceneData);
		m_ComputeBuffer = rhi->CreateBuffer(&computeDataDesc, &m_ComputeData);

		TextureDesc computeOutputDesc = {};
		computeOutputDesc.data = nullptr;
		computeOutputDesc.width = static_cast<uint32_t>(320);
		computeOutputDesc.height = static_cast<uint32_t>(180);
		computeOutputDesc.bIsStorage = true;
		computeOutputDesc.bGenerateMips = false;
		m_OutputTexture = rhi->CreateTexture(&computeOutputDesc);

		m_RenderTimer = rhi->CreateGPUTimer();
		m_ComputeTimer = rhi->CreateGPUTimer();
	}

	std::pair<uint32_t, uint32_t> GetViewportMousePos()
	{
		std::pair<uint32_t, uint32_t> viewportMousePos;
		auto pos = Input::GetMousePos();
		viewportMousePos.first = static_cast<uint32_t>(pos.first - m_ViewportPos.x);
		viewportMousePos.second = static_cast<uint32_t>(pos.second - m_ViewportPos.y);

		return viewportMousePos;
	}

	void PrepareScene()
	{
		if (m_CurrentScene->IsSceneLoaded())
		{
			m_CurrentScene->ExecutePreparations();
			return;
		}

		auto& sceneToLoad = m_CurrentScene->GetScenePath();

		m_CurrentScene->GetSelectedEntity().Invalidate();
		edelete m_CurrentScene;
		m_CurrentScene = enew Scene();
	#if WITH_EDITOR
		m_SceneHierarchy->SetCurrentScene(m_CurrentScene);
	#endif


		if (!sceneToLoad.empty())
		{
			SceneSerializer serializer(m_CurrentScene);
			serializer.Deserialize(sceneToLoad);

			auto entities = m_CurrentScene->GetAllEntitiesWith<MeshComponent>();
			for (auto& entity : entities)
			{
				Entity e = { entity, m_CurrentScene };
				e.GetComponent<MeshComponent>().LoadMeshSource(rhi);
			}

			m_CurrentScene->SetScenePath(sceneToLoad);
		}
		
		m_CurrentScene->SetSceneLoaded(true);
		ChangeWindowTitle(m_CurrentScene->GetName());
	}

	void NewScene()
	{
		m_CurrentScene->SetScenePath("");
		m_CurrentScene->SetSceneLoaded(false);
	}

	void OpenScene(const std::filesystem::path& path)
	{
		m_CurrentScene->SetScenePath(path);
		m_CurrentScene->SetSceneLoaded(false);
		m_Camera = Camera(window->GetWidth(), window->GetHeight()); // Reset camera
	}

	void OpenSceneDialog()
	{
		std::string path = OpenFileDialog("Eden Scene (.escene)\0*.escene\0");
		if (!path.empty())
			OpenScene(path);
	}

	void SaveSceneAs()
	{
		std::filesystem::path path = SaveFileDialog("Eden Scene (.escene)\0*.escene\0");
		if (!path.empty())
		{
			if (!path.has_extension())
				path += Utils::ExtensionToString(EdenExtension::kScene);

			SceneSerializer serializer(m_CurrentScene);
			serializer.Serialize(path);
			m_CurrentScene->SetScenePath(path);
			ChangeWindowTitle(m_CurrentScene->GetName());
		}
	}

	void SaveScene()
	{
		if (m_CurrentScene->GetScenePath().empty())
		{
			SaveSceneAs();
			return;
		}

		ED_LOG_INFO("Saved scene: {}", m_CurrentScene->GetScenePath());
		SceneSerializer serializer(m_CurrentScene);
		serializer.Serialize(m_CurrentScene->GetScenePath());
	}

	void UI_Dockspace()
	{
		ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;
		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("EdenDockSpace", nullptr, windowFlags);
		ImGui::PopStyleVar(3);

		// Submit the DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspaceId = ImGui::GetID("EdenEditorDockspace");
			ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);
		}

		// Menubar
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New Scene", "Ctrl+N"))
					NewScene();

				if (ImGui::MenuItem("Open Scene", "Ctrl+O"))
					OpenSceneDialog();

				if (ImGui::MenuItem("Save Scene", "Ctrl-S"))
					SaveScene();

				if (ImGui::MenuItem("Save Scene As", "Ctrl+Shift+S"))
					SaveSceneAs();

				if (ImGui::MenuItem("Exit"))
					window->CloseWasRequested();

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View"))
			{
				ImGui::MenuItem("Statistics", NULL, &m_bOpenStatisticsWindow);
				ImGui::MenuItem("Inspector", NULL, &m_SceneHierarchy->bOpenInspector);
				ImGui::MenuItem("Hierarchy", NULL, &m_SceneHierarchy->bOpenHierarchy);
				ImGui::MenuItem("Scene Properties", NULL, &m_bOpenSceneProperties);
				ImGui::MenuItem("Pipelines Panel", NULL, &m_bOpenPipelinesPanel);
				ImGui::MenuItem("Content Browser", NULL, &m_ContentBrowserPanel->bOpenContentBrowser);

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Entity"))
			{
				m_SceneHierarchy->EntityMenu();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Options"))
			{
				ImGui::PushItemWidth(150.0f);
				ImGui::Text("Theme:");
				ImGui::SameLine();
				if (ImGui::Combo("###theme", reinterpret_cast<int*>(&UI::g_SelectedTheme), "Cherry\0Hazel\0"))
				{
					switch (UI::g_SelectedTheme)
					{
					case UI::Themes::Cherry: UI::CherryTheme(); break;
					case UI::Themes::Hazel: UI::HazelTheme(); break;
					}
				}

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		ImGui::End();
	}

	void UI_SceneProperties()
	{
		ImGui::Begin(ICON_FA_TV " Scene Properties##sceneProperties", &m_bOpenSceneProperties, ImGuiWindowFlags_NoCollapse);
		ImGui::Text("Compute Shader test:");
		ImVec2 imageSize;
		imageSize.x = static_cast<float>(m_OutputTexture->desc.width);
		imageSize.y = static_cast<float>(m_OutputTexture->desc.height);
		rhi->ensureMsgResourceState(m_OutputTexture, ResourceState::PIXEL_SHADER);
		ImGui::Image((ImTextureID)rhi->GetTextureID(m_OutputTexture), imageSize);
		ImGui::Separator();

		ImGui::Checkbox("Enable Skybox", &m_bIsSkyboxEnabled);
		ImGui::Separator();
		UI::DrawProperty("Exposure", m_SceneSettings.exposure, 0.1f, 0.1f, 5.0f);
		ImGui::End();
	}

	void UI_StatisticsWindow()
	{
		ImGui::Begin(ICON_FA_CHART_PIE " Statistcs##statistics", &m_bOpenStatisticsWindow, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Text("CPU frame time: %.3fms(%.1fFPS)", deltaTime * 1000.0f, (1000.0f / deltaTime) / 1000.0f);
		ImGui::Text("GPU frame time: %.3fms", m_RenderTimer->elapsedTime);
		ImGui::Text("Compute test time: %.3fms", m_ComputeTimer->elapsedTime);
		ImGui::End();
	}

	void UI_DrawGizmo()
	{
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();

		float windowWidth = m_ViewportSize.x;
		float windowHeight = m_ViewportSize.y;
		float windowX = m_ViewportPos.x;
		float windowY = m_ViewportPos.y;
		ImGuizmo::SetRect(windowX, windowY, windowWidth, windowHeight);

		auto& transform_component = m_CurrentScene->GetSelectedEntity().GetComponent<TransformComponent>();
		glm::mat4 transform = transform_component.GetTransform();

		ImGuizmo::Manipulate(glm::value_ptr(m_ViewMatrix), glm::value_ptr(m_ProjectionMatrix), static_cast<ImGuizmo::OPERATION>(m_GizmoType), ImGuizmo::LOCAL, glm::value_ptr(transform));

		if (ImGuizmo::IsUsing())
		{
			glm::vec3 translation, rotation, scale;
			Math::DecomposeTransform(transform, translation, rotation, scale);

			glm::vec3 deltaRotation = rotation - transform_component.rotation;
			transform_component.translation = translation;
			transform_component.rotation = rotation;
			transform_component.scale = scale;
		}
	}

	void UI_PipelinesPanel()
	{
		ImGui::Begin(ICON_FA_PUZZLE_PIECE " Pipelines##pipelines");
		ImGui::Columns(2);
		ImGui::NextColumn();
		if (ImGui::Button("Reload all pipelines"))
		{
			for (auto& pipeline : m_Pipelines)
				m_CurrentScene->AddPreparation([&]() {rhi->ReloadPipeline(pipeline.second); });
		}
		ImGui::Columns(1);
		for (auto& pipeline : m_Pipelines)
		{
			ImGui::PushID(pipeline.second->debugName.c_str());
			ImGui::Columns(2);
			ImGui::Text(pipeline.first);
			ImGui::NextColumn();
			if (ImGui::Button("Reload pipeline"))
				m_CurrentScene->AddPreparation([&]() {rhi->ReloadPipeline(pipeline.second); });
			ImGui::Columns(1);
			ImGui::PopID();
		}
		ImGui::End();
	}

	void UI_Viewport()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoTitleBar);

		if (ImGui::IsWindowHovered())
			Input::SetInputMode(InputMode::Game);
		else
			Input::SetInputMode(InputMode::UI);

		// Focus window on right click
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1))
			ImGui::SetWindowFocus();

		auto viewportSize = ImGui::GetContentRegionAvail();
		if (m_ViewportSize != *(glm::vec2*)&viewportSize)
		{
			m_ViewportSize = { viewportSize.x, viewportSize.y };
			m_GBuffer->desc.width = (uint32_t)m_ViewportSize.x;
			m_GBuffer->desc.height = (uint32_t)m_ViewportSize.y;
			m_SceneComposite->desc.width = (uint32_t)m_ViewportSize.x;
			m_SceneComposite->desc.height = (uint32_t)m_ViewportSize.y;
			m_ObjectPickerPass->desc.width = (uint32_t)m_ViewportSize.x;
			m_ObjectPickerPass->desc.height = (uint32_t)m_ViewportSize.y;
			m_Camera.SetViewportSize(m_ViewportSize);
		}
		auto viewportPos = ImGui::GetWindowPos();
		m_ViewportPos = { viewportPos.x, viewportPos.y };
		m_Camera.SetViewportPosition(m_ViewportPos);

		m_bIsViewportFocused = ImGui::IsWindowFocused();
		m_bIsViewportHovered = ImGui::IsWindowHovered();

		ImGui::Image((ImTextureID)rhi->GetTextureID(m_SceneComposite->colorAttachments[0]), viewportSize);

		// Drag and drop
		if (ImGui::BeginDragDropTarget())
		{
			if (auto payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP))
			{
				std::filesystem::path path = std::filesystem::path(reinterpret_cast<const char*>(payload->Data));
				EdenExtension extension = Utils::StringToExtension(path.extension().string());

				switch (extension)
				{
				case EdenExtension::kScene:
					OpenScene(path);
					break;
				case EdenExtension::kModel:
					{
						auto e = m_CurrentScene->CreateEntity(path.stem().string());
						auto& mc = e.AddComponent<MeshComponent>();
						mc.meshPath = path.string();
						m_CurrentScene->AddPreparation([&]() {
							mc.LoadMeshSource(rhi);
						});
					}
					break;
				case EdenExtension::kEnvironmentMap:
					m_Skybox->SetNewTexture(path.string().c_str());
					m_CurrentScene->AddPreparation([&]() {
						m_Skybox->UpdateNewTexture(rhi);
					});
					break;
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (Input::GetKeyDown(ED_KEY_DELETE) && ImGui::IsWindowFocused())
		{
			m_CurrentScene->AddPreparation([&]() {
				auto selectedEntity = m_CurrentScene->GetSelectedEntity();
				m_CurrentScene->DeleteEntity(selectedEntity);
			});
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void UI_MemoryPanel()
	{
		auto stats = Memory::GetAllocationStats();

		std::string total_allocated = "Total Allocated: " + Utils::BytesToString(stats.total_allocated);
		std::string total_freed = "Total Freed: " + Utils::BytesToString(stats.total_freed);
		std::string currentUsage = "Current Usage: " +  Utils::BytesToString(stats.total_allocated - stats.total_freed);

		ImGui::Begin(ICON_FA_BUG " Memory##memory", &m_bOpenMemoryPanel);
		ImGui::Text(total_allocated.c_str());
		ImGui::Text(total_freed.c_str());
		ImGui::Text(currentUsage.c_str());
		ImGui::End();
	}

	void UI_Render()
	{
		//ImGui::ShowDemoWindow(nullptr);

		UI_Dockspace();
		UI_Viewport();
		m_ContentBrowserPanel->Render();
		m_SceneHierarchy->Render();
		if (m_bOpenStatisticsWindow)
			UI_StatisticsWindow();
		if (m_CurrentScene->GetSelectedEntity())
			UI_DrawGizmo();
		if (m_bOpenPipelinesPanel)
			UI_PipelinesPanel();
		if (m_bOpenSceneProperties)
			UI_SceneProperties();
		if (m_bOpenMemoryPanel)
			UI_MemoryPanel();
	}

	void EditorInput()
	{
		if (!Input::GetMouseButton(MouseButton::RightButton) && m_bIsViewportFocused)
		{
			if (Input::GetKey(KeyCode::Q))
				m_GizmoType = -1;
			if (Input::GetKey(KeyCode::W))
				m_GizmoType = ImGuizmo::TRANSLATE;
			if (Input::GetKey(KeyCode::E))
				m_GizmoType = ImGuizmo::ROTATE;
			if (Input::GetKey(KeyCode::R))
				m_GizmoType = ImGuizmo::SCALE;
		}

		if (Input::GetMouseButtonDown(MouseButton::LeftButton))
		{
			if (m_bIsViewportHovered && !ImGuizmo::IsOver())
			{
				auto mousePos = GetViewportMousePos();
				glm::vec4 pixel_color;
				if (mousePos.first >= 1 && mousePos.first <= m_ViewportSize.x && mousePos.second >= 1 && mousePos.second <= m_ViewportSize.y)
					rhi->ReadPixelFromTexture(mousePos.first, mousePos.second, m_ObjectPickerPass->colorAttachments[0], pixel_color);
				if (pixel_color.x >= 0)
				{
					entt::entity entityId = static_cast<entt::entity>(std::round(pixel_color.x));
					Entity entity = { entityId, m_CurrentScene };
					m_CurrentScene->SetSelectedEntity(entity);
				}
				else
				{
					entt::entity entityId = static_cast<entt::entity>(-1);
					Entity entity = { entityId, m_CurrentScene };
					m_CurrentScene->SetSelectedEntity(entity);
				}
			}
		}

		if (Input::GetKey(ED_KEY_SHIFT) && Input::GetKey(ED_KEY_CONTROL) && Input::GetKeyDown(ED_KEY_S))
			SaveSceneAs();
		if (Input::GetKey(ED_KEY_CONTROL) && Input::GetKeyDown(ED_KEY_O))
			OpenSceneDialog();
		if (Input::GetKey(ED_KEY_CONTROL) && Input::GetKeyDown(ED_KEY_N))
			NewScene();
		if (Input::GetKey(ED_KEY_CONTROL) && Input::GetKeyDown(ED_KEY_S))
			SaveScene();
	}

	void UpdatePointLights()
	{
		PointLightComponent emptyPl;
		emptyPl.color = glm::vec4(0);

		std::array<PointLightComponent, MAX_POINT_LIGHTS> pointLightComponents;
		pointLightComponents.fill(emptyPl);

		auto pointLights = m_CurrentScene->GetAllEntitiesWith<PointLightComponent>();
		for (int i = 0; i < pointLights.size(); ++i)
		{
			Entity e = { pointLights[i], m_CurrentScene};
			PointLightComponent& pl = e.GetComponent<PointLightComponent>();
			pointLightComponents[i] = pl;
		}

		const void* data = pointLightComponents.data();
		rhi->UpdateBufferData(m_PointLightsBuffer, data);
	}

	void UpdateDirectionalLights()
	{
		DirectionalLightComponent emptyDl;
		emptyDl.intensity = 0;

		std::array<DirectionalLightComponent, MAX_DIRECTIONAL_LIGHTS> directional_lightComponents;
		directional_lightComponents.fill(emptyDl);

		auto directional_lights = m_CurrentScene->GetAllEntitiesWith<DirectionalLightComponent>();
		for (int i = 0; i < directional_lights.size(); ++i)
		{
			Entity e = { directional_lights[i], m_CurrentScene };
			DirectionalLightComponent& pl = e.GetComponent<DirectionalLightComponent>();
			directional_lightComponents[i] = pl;
		}

		if (directional_lights.size() > 0)
			m_DirectionalLight = { directional_lights[0], m_CurrentScene };

		const void* data = directional_lightComponents.data();
		rhi->UpdateBufferData(m_DirectionalLightsBuffer, data);
	}

	void ObjectPickerPass()
	{
		auto entitiesToRender = m_CurrentScene->GetAllEntitiesWith<MeshComponent>();

		rhi->BeginRenderPass(m_ObjectPickerPass);
		rhi->BindPipeline(m_Pipelines["Object Picker"]);
		for (auto entity : entitiesToRender)
		{
			Entity e = { entity, m_CurrentScene };
			std::shared_ptr<MeshSource> ms = e.GetComponent<MeshComponent>().meshSource;
			if (!ms->bHasMesh)
				continue;
			TransformComponent tc = e.GetComponent<TransformComponent>();

			rhi->BindVertexBuffer(ms->meshVb);
			rhi->BindIndexBuffer(ms->meshIb);
			for (auto& mesh : ms->meshes)
			{
				auto transform = tc.GetTransform();
				if (mesh->gltfMatrix != glm::mat4(1.0f))
					transform *= mesh->gltfMatrix;

				rhi->BindParameter("SceneData", m_SceneDataCB);
				rhi->BindParameter("Transform", &transform, sizeof(glm::mat4));
				rhi->BindParameter("ObjectID", &entity, sizeof(uint32_t));

				for (auto& submesh : mesh->submeshes)
					rhi->DrawIndexed(submesh->indexCount, 1, submesh->indexStart);
			}
		}
		rhi->EndRenderPass(m_ObjectPickerPass);
	}

	void MainColorPass()
	{
		auto entitiesToRender = m_CurrentScene->GetAllEntitiesWith<MeshComponent>();

		// GBuffer
		rhi->BeginRenderPass(m_GBuffer);
		rhi->BindPipeline(m_Pipelines["Phong Lighting"]);
		for (auto entity : entitiesToRender)
		{
			Entity e = { entity, m_CurrentScene };
			std::shared_ptr<MeshSource> ms = e.GetComponent<MeshComponent>().meshSource;
			if (!ms->bHasMesh)
				continue;
			TransformComponent tc = e.GetComponent<TransformComponent>();

			rhi->BindVertexBuffer(ms->meshVb);
			rhi->BindIndexBuffer(ms->meshIb);
			for (auto& mesh : ms->meshes)
			{
				auto transform = tc.GetTransform();
				if (mesh->gltfMatrix != glm::mat4(1.0f))
					transform *= mesh->gltfMatrix;

				rhi->BindParameter("SceneData", m_SceneDataCB);
				rhi->BindParameter("Transform", &transform, sizeof(glm::mat4));
				rhi->BindParameter("DirectionalLights", m_DirectionalLightsBuffer);
				rhi->BindParameter("PointLights", m_PointLightsBuffer);

				for (auto& submesh : mesh->submeshes)
				{
					rhi->BindParameter("g_TextureDiffuse", submesh->diffuseTexture);
					rhi->BindParameter("g_TextureEmissive", submesh->emissiveTexture);
					rhi->DrawIndexed(submesh->indexCount, 1, submesh->indexStart);
				}
			}
		}

		// Skybox
		if (m_bIsSkyboxEnabled)
		{
			rhi->BindPipeline(m_Pipelines["Skybox"]);
			m_Skybox->Render(rhi, m_ProjectionMatrix * glm::mat4(glm::mat3(m_ViewMatrix)));
		}
		rhi->EndRenderPass(m_GBuffer);
	}

	void SceneCompositePass()
	{
		// Scene Composite
		rhi->BeginRenderPass(m_SceneComposite);
		rhi->BindPipeline(m_Pipelines["Scene Composite"]);
		rhi->BindVertexBuffer(m_QuadBuffer);
		rhi->BindParameter("g_SceneTexture", m_GBuffer->colorAttachments[0]);
		rhi->UpdateBufferData(m_SceneSettingsBuffer, &m_SceneSettings);
		rhi->BindParameter("SceneSettings", m_SceneSettingsBuffer);
		rhi->Draw(6);
		rhi->EndRenderPass(m_SceneComposite);
	}

	void OnUpdate() override
	{
		ED_PROFILE_FUNCTION();

	#if !WITH_EDITOR
		m_ViewportSize = { window->GetWidth(), window->GetHeight() };
	#endif

		PrepareScene();

		// Update camera and scene data
		m_Camera.Update(deltaTime);
		m_ViewMatrix = glm::lookAtLH(m_Camera.position, m_Camera.position + m_Camera.front, m_Camera.up);
		m_ProjectionMatrix = glm::perspectiveFovLH(glm::radians(70.0f), m_ViewportSize.x, m_ViewportSize.y, 0.1f, 200.0f);
		m_SceneData.view = m_ViewMatrix;
		m_SceneData.viewProjection = m_ProjectionMatrix * m_ViewMatrix;
		m_SceneData.viewPosition = glm::vec4(m_Camera.position, 1.0f);
		rhi->UpdateBufferData(m_SceneDataCB, &m_SceneData);
		
		EditorInput();
		
		UpdateDirectionalLights();
		UpdatePointLights();

		rhi->BeginRender();
		rhi->BeginGPUTimer(m_RenderTimer);

		// Compute shader test
		rhi->BeginGPUTimer(m_ComputeTimer);
		rhi->BindPipeline(m_Pipelines["Cubes Test"]);
		m_ComputeData.resolution = glm::vec2(m_OutputTexture->desc.width, m_OutputTexture->desc.height);
		m_ComputeData.time = creationTime;
		rhi->UpdateBufferData(m_ComputeBuffer, &m_ComputeData);
		rhi->BindParameter("RenderingInfo", m_ComputeBuffer);
		rhi->BindParameter("OutputTexture", m_OutputTexture, kReadWrite);
		//! 8 is the num of threads, changing in there requires to change in shader
		rhi->Dispatch(static_cast<uint32_t>(m_OutputTexture->desc.width / 8), static_cast<uint32_t>(m_OutputTexture->desc.height / 8), 1); // TODO: abstract the num of threads in some way
		rhi->EndGPUTimer(m_ComputeTimer);

		ObjectPickerPass();
		MainColorPass();
		SceneCompositePass();
		

	#if WITH_EDITOR
		rhi->BeginRenderPass(m_ImGuiPass);
		UI_Render();
		rhi->EndRenderPass(m_ImGuiPass);
	#endif

		rhi->EndGPUTimer(m_RenderTimer);
		rhi->EndRender();
		rhi->Render();
	}


	void OnDestroy() override
	{
		edelete m_CurrentScene;
	}

};
EdenApplication* app;

int EdenMain()
{
	app = enew EdenApplication();
	app->Run();
	edelete app;

	return 0;
}