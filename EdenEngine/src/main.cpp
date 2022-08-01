#include "Core/Window.h"
#include "Core/Memory.h"
#include "Core/Input.h"
#include "Core/Log.h"
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
		glm::mat4 view_projection;
		glm::vec4 view_position;
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
	bool m_SkyboxEnable = true;

	// UI
	bool m_OpenStatisticsWindow = true;
	bool m_OpenSceneProperties = true;
	bool m_OpenPipelinesPanel = true;
	bool m_OpenMemoryPanel = true;
	int m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
	glm::vec2 m_ViewportSize;
	glm::vec2 m_ViewportPos;
	bool m_ViewportFocused = false;
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
		m_SceneData.view_projection = m_ProjectionMatrix * m_ViewMatrix;
		m_SceneData.view_position = glm::vec4(m_Camera.position, 1.0f);

		BufferDesc scene_data_desc = {};
		scene_data_desc.element_count = 1;
		scene_data_desc.stride = sizeof(SceneData);
		m_SceneDataCB = rhi->CreateBuffer(&scene_data_desc, &m_SceneData);

		m_CurrentScene = enew Scene();
		std::string default_scene = "assets/scenes/cube.escene";
		OpenScene(default_scene);

		m_Skybox = std::make_shared<Skybox>(rhi, "assets/skyboxes/san_giuseppe_bridge.hdr");

		// Lights
		BufferDesc dl_desc = {};
		dl_desc.element_count = MAX_DIRECTIONAL_LIGHTS;
		dl_desc.stride = sizeof(DirectionalLightComponent);
		dl_desc.usage = BufferDesc::Storage;
		m_DirectionalLightsBuffer = rhi->CreateBuffer(&dl_desc, nullptr);
		UpdateDirectionalLights();

		BufferDesc pl_desc = {};
		pl_desc.element_count = MAX_POINT_LIGHTS;
		pl_desc.stride = sizeof(PointLightComponent);
		pl_desc.usage = BufferDesc::Storage;
		m_PointLightsBuffer = rhi->CreateBuffer(&pl_desc, nullptr);
		UpdatePointLights();

		// GBuffer
		{
			RenderPassDesc desc = {};
			desc.debug_name = "GBuffer";
			desc.attachments_formats = { Format::RGBA32_FLOAT, Format::Depth };
			desc.width = static_cast<uint32_t>(m_ViewportSize.x);
			desc.height = static_cast<uint32_t>(m_ViewportSize.y);
			m_GBuffer = rhi->CreateRenderPass(&desc);

			PipelineDesc phong_lighting_desc = {};
			phong_lighting_desc.enable_blending = true;
			phong_lighting_desc.program_name = "PhongLighting";
			phong_lighting_desc.render_pass = m_GBuffer;
			m_Pipelines["Phong Lighting"] = rhi->CreatePipeline(&phong_lighting_desc);

			PipelineDesc skybox_desc = {};
			skybox_desc.cull_mode = CullMode::NONE;
			skybox_desc.depth_func = ComparisonFunc::LESS_EQUAL;
			skybox_desc.min_depth = 1.0f;
			skybox_desc.program_name = "Skybox";
			skybox_desc.render_pass = m_GBuffer;
			m_Pipelines["Skybox"] = rhi->CreatePipeline(&skybox_desc);
		}

	#if WITH_EDITOR
		// Scene Composite
		{
			RenderPassDesc desc = {};
			desc.debug_name = "SceneComposite";
			desc.attachments_formats = { Format::RGBA32_FLOAT, Format::Depth };
			desc.width = static_cast<uint32_t>(m_ViewportSize.x);
			desc.height = static_cast<uint32_t>(m_ViewportSize.y);
			m_SceneComposite = rhi->CreateRenderPass(&desc);

			PipelineDesc scene_composite_desc = {};
			scene_composite_desc.program_name = "SceneComposite";
			scene_composite_desc.render_pass = m_SceneComposite;
			m_Pipelines["Scene Composite"] = rhi->CreatePipeline(&scene_composite_desc);
		}

		{
			RenderPassDesc desc = {};
			desc.debug_name = "ImGuiPass";
			desc.attachments_formats = { Format::RGBA32_FLOAT, Format::Depth };
			desc.swapchain_target = true;
			desc.imgui_pass = true;
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
			desc.debug_name = "SceneComposite";
			desc.attachments_formats = { Format::RGBA32_FLOAT, Format::Depth };
			desc.swapchain_target = true;
			desc.width = static_cast<uint32_t>(m_ViewportSize.x);
			desc.height = static_cast<uint32_t>(m_ViewportSize.y);
			m_SceneComposite = rhi->CreateRenderPass(&desc);
			rhi->SetSwapchainTarget(m_SceneComposite);

			PipelineDesc scene_composite_desc = {};
			scene_composite_desc.program_name = "SceneComposite";
			scene_composite_desc.render_pass = m_SceneComposite;
			m_Pipelines["Scene Composite"] = rhi->CreatePipeline(&scene_composite_desc);
		}
	#endif

		PipelineDesc cubes_test_desc = {};
		cubes_test_desc.program_name = "CubesCS";
		cubes_test_desc.type = Compute;
		m_Pipelines["Cubes Test"] = rhi->CreatePipeline(&cubes_test_desc);

		float quad_vertices[] = {
			// positions   // texCoords
			-1.0f,  1.0f,   0.0f, 1.0f,
			-1.0f, -1.0f,   0.0f, 0.0f,
			 1.0f, -1.0f,  -1.0f, 0.0f,

			-1.0f,  1.0f,   0.0f, 1.0f,
			 1.0f, -1.0f,  -1.0f, 0.0f,
			 1.0f,  1.0f,  -1.0f, 1.0f
		};
		BufferDesc quad_desc = {};
		quad_desc.stride = sizeof(float) * 4;
		quad_desc.element_count = 6;
		quad_desc.usage = BufferDesc::Vertex_Index;
		m_QuadBuffer = rhi->CreateBuffer(&quad_desc, quad_vertices);

		BufferDesc scene_settings_desc = {};
		scene_settings_desc.element_count = 1;
		scene_settings_desc.stride = sizeof(SceneSettings);
		scene_settings_desc.usage = BufferDesc::Uniform;
		m_SceneSettingsBuffer = rhi->CreateBuffer(&scene_settings_desc, &m_SceneSettings);

		// Compute shader
		m_ComputeData.resolution = glm::vec2(m_ViewportSize.x, m_ViewportSize.y);
		BufferDesc compute_data_desc = {};
		compute_data_desc.element_count = 1;
		compute_data_desc.stride = sizeof(SceneData);
		m_ComputeBuffer = rhi->CreateBuffer(&compute_data_desc, &m_ComputeData);

		TextureDesc compute_output_desc = {};
		compute_output_desc.data = nullptr;
		compute_output_desc.width = static_cast<uint32_t>(320);
		compute_output_desc.height = static_cast<uint32_t>(180);
		compute_output_desc.storage = true;
		compute_output_desc.generate_mips = false;
		m_OutputTexture = rhi->CreateTexture(&compute_output_desc);

		m_RenderTimer = rhi->CreateGPUTimer();
		m_ComputeTimer = rhi->CreateGPUTimer();
	}

	void PrepareScene()
	{
		if (m_CurrentScene->IsSceneLoaded())
		{
			m_CurrentScene->ExecutePreparations();
			return;
		}

		auto& scene_to_load = m_CurrentScene->GetScenePath();

		m_CurrentScene->GetSelectedEntity().Invalidate();
		edelete m_CurrentScene;
		m_CurrentScene = enew Scene();
	#if WITH_EDITOR
		m_SceneHierarchy->SetCurrentScene(m_CurrentScene);
	#endif


		if (!scene_to_load.empty())
		{
			SceneSerializer serializer(m_CurrentScene);
			serializer.Deserialize(scene_to_load);

			auto entities = m_CurrentScene->GetAllEntitiesWith<MeshComponent>();
			for (auto& entity : entities)
			{
				Entity e = { entity, m_CurrentScene };
				e.GetComponent<MeshComponent>().LoadMeshSource(rhi);
			}

			m_CurrentScene->SetScenePath(scene_to_load);
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
				path += SceneSerializer::DefaultExtension;

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
		ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("EdenDockSpace", nullptr, window_flags);
		ImGui::PopStyleVar(3);

		// Submit the DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("EdenEditorDockspace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
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
				ImGui::MenuItem("Statistics", NULL, &m_OpenStatisticsWindow);
				ImGui::MenuItem("Entity Properties", NULL, &m_SceneHierarchy->open_entity_properties);
				ImGui::MenuItem("Scene Hierarchy", NULL, &m_SceneHierarchy->open_scene_hierarchy);
				ImGui::MenuItem("Scene Properties", NULL, &m_OpenSceneProperties);
				ImGui::MenuItem("Pipelines Panel", NULL, &m_OpenPipelinesPanel);
				ImGui::MenuItem("Content Browser", NULL, &m_ContentBrowserPanel->open_content_browser);

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
		ImGui::Begin(ICON_FA_TV " Scene Properties##scene_properties", &m_OpenSceneProperties, ImGuiWindowFlags_NoCollapse);
		ImGui::Text("Compute Shader test:");
		ImVec2 image_size;
		image_size.x = static_cast<float>(m_OutputTexture->desc.width);
		image_size.y = static_cast<float>(m_OutputTexture->desc.height);
		rhi->EnsureResourceState(m_OutputTexture, ResourceState::PIXEL_SHADER);
		ImGui::Image((ImTextureID)rhi->GetTextureID(m_OutputTexture), image_size);
		ImGui::Separator();

		ImGui::Checkbox("Enable Skybox", &m_SkyboxEnable);
		ImGui::Separator();
		UI::DrawProperty("Exposure", m_SceneSettings.exposure, 0.1f, 0.1f, 5.0f);
		ImGui::End();
	}

	void UI_StatisticsWindow()
	{
		ImGui::Begin(ICON_FA_CHART_PIE " Statistcs##statistics", &m_OpenStatisticsWindow, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Text("CPU frame time: %.3fms(%.1fFPS)", delta_time * 1000.0f, (1000.0f / delta_time) / 1000.0f);
		ImGui::Text("GPU frame time: %.3fms", m_RenderTimer->elapsed_time);
		ImGui::Text("Compute test time: %.3fms", m_ComputeTimer->elapsed_time);
		ImGui::End();
	}

	void UI_DrawGizmo()
	{
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();

		float window_width = m_ViewportSize.x;
		float window_height = m_ViewportSize.y;
		float window_x = m_ViewportPos.x;
		float window_y = m_ViewportPos.y;
		ImGuizmo::SetRect(window_x, window_y, window_width, window_height);

		auto& transform_component = m_CurrentScene->GetSelectedEntity().GetComponent<TransformComponent>();
		glm::mat4 transform = transform_component.GetTransform();

		ImGuizmo::Manipulate(glm::value_ptr(m_ViewMatrix), glm::value_ptr(m_ProjectionMatrix), static_cast<ImGuizmo::OPERATION>(m_GizmoType), ImGuizmo::LOCAL, glm::value_ptr(transform));

		if (ImGuizmo::IsUsing())
		{
			glm::vec3 translation, rotation, scale;
			Math::DecomposeTransform(transform, translation, rotation, scale);

			glm::vec3 delta_rotation = rotation - transform_component.rotation;
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
			ImGui::PushID(pipeline.second->debug_name.c_str());
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

		auto viewport_size = ImGui::GetContentRegionAvail();
		if (m_ViewportSize != *(glm::vec2*)&viewport_size)
		{
			m_ViewportSize = { viewport_size.x, viewport_size.y };
			m_GBuffer->desc.width = (uint32_t)m_ViewportSize.x;
			m_GBuffer->desc.height = (uint32_t)m_ViewportSize.y;
			m_SceneComposite->desc.width = (uint32_t)m_ViewportSize.x;
			m_SceneComposite->desc.height = (uint32_t)m_ViewportSize.y;
			m_Camera.SetViewportSize(m_ViewportSize);
		}
		auto viewport_pos = ImGui::GetWindowPos();
		m_ViewportPos = { (uint32_t)viewport_pos.x, (uint32_t)viewport_pos.y };
		m_Camera.SetViewportPosition(m_ViewportPos);

		m_ViewportFocused = ImGui::IsWindowFocused();

		ImGui::Image((ImTextureID)rhi->GetTextureID(m_SceneComposite->color_attachments[0]), viewport_size);

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
						mc.mesh_path = path.string();
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
				auto selected_entity = m_CurrentScene->GetSelectedEntity();
				m_CurrentScene->DeleteEntity(selected_entity);
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
		std::string current_usage = "Current Usage: " +  Utils::BytesToString(stats.total_allocated - stats.total_freed);

		ImGui::Begin(ICON_FA_BUG " Memory##memory", &m_OpenMemoryPanel);
		ImGui::Text(total_allocated.c_str());
		ImGui::Text(total_freed.c_str());
		ImGui::Text(current_usage.c_str());
		ImGui::End();
	}

	void UI_Render()
	{
		//ImGui::ShowDemoWindow(nullptr);

		UI_Dockspace();
		UI_Viewport();
		m_ContentBrowserPanel->Render();
		m_SceneHierarchy->Render();
		if (m_OpenStatisticsWindow)
			UI_StatisticsWindow();
		if (m_CurrentScene->GetSelectedEntity())
			UI_DrawGizmo();
		if (m_OpenPipelinesPanel)
			UI_PipelinesPanel();
		if (m_OpenSceneProperties)
			UI_SceneProperties();
		if (m_OpenMemoryPanel)
			UI_MemoryPanel();
	}

	void EditorInput()
	{
		if (!Input::GetMouseButton(MouseButton::RightButton) && m_ViewportFocused)
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
		PointLightComponent empty_pl;
		empty_pl.color = glm::vec4(0);

		std::array<PointLightComponent, MAX_POINT_LIGHTS> point_light_components;
		point_light_components.fill(empty_pl);

		auto point_lights = m_CurrentScene->GetAllEntitiesWith<PointLightComponent>();
		for (int i = 0; i < point_lights.size(); ++i)
		{
			Entity e = { point_lights[i], m_CurrentScene};
			PointLightComponent& pl = e.GetComponent<PointLightComponent>();
			point_light_components[i] = pl;
		}

		const void* data = point_light_components.data();
		rhi->UpdateBufferData(m_PointLightsBuffer, data);
	}

	void UpdateDirectionalLights()
	{
		DirectionalLightComponent empty_dl;
		empty_dl.intensity = 0;

		std::array<DirectionalLightComponent, MAX_DIRECTIONAL_LIGHTS> directional_light_components;
		directional_light_components.fill(empty_dl);

		auto directional_lights = m_CurrentScene->GetAllEntitiesWith<DirectionalLightComponent>();
		for (int i = 0; i < directional_lights.size(); ++i)
		{
			Entity e = { directional_lights[i], m_CurrentScene };
			DirectionalLightComponent& pl = e.GetComponent<DirectionalLightComponent>();
			directional_light_components[i] = pl;
		}

		if (directional_lights.size() > 0)
			m_DirectionalLight = { directional_lights[0], m_CurrentScene };

		const void* data = directional_light_components.data();
		rhi->UpdateBufferData(m_DirectionalLightsBuffer, data);
	}

	void OnUpdate() override
	{
		ED_PROFILE_FUNCTION();

	#if !WITH_EDITOR
		m_ViewportSize = { window->GetWidth(), window->GetHeight() };
	#endif

		PrepareScene();

		// Update camera and scene data
		m_Camera.Update(delta_time);
		m_ViewMatrix = glm::lookAtLH(m_Camera.position, m_Camera.position + m_Camera.front, m_Camera.up);
		m_ProjectionMatrix = glm::perspectiveFovLH(glm::radians(70.0f), m_ViewportSize.x, m_ViewportSize.y, 0.1f, 200.0f);
		m_SceneData.view = m_ViewMatrix;
		m_SceneData.view_projection = m_ProjectionMatrix * m_ViewMatrix;
		m_SceneData.view_position = glm::vec4(m_Camera.position, 1.0f);
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
		m_ComputeData.time = creation_time;
		rhi->UpdateBufferData(m_ComputeBuffer, &m_ComputeData);
		rhi->BindParameter("RenderingInfo", m_ComputeBuffer);
		rhi->BindParameter("OutputTexture", m_OutputTexture, kReadWrite);
		//! 8 is the num of threads, changing in there requires to change in shader
		rhi->Dispatch(static_cast<uint32_t>(m_OutputTexture->desc.width / 8), static_cast<uint32_t>(m_OutputTexture->desc.height / 8), 1); // TODO: abstract the num of threads in some way
		rhi->EndGPUTimer(m_ComputeTimer);

		auto entities_to_render = m_CurrentScene->GetAllEntitiesWith<MeshComponent>();

		// GBuffer
		rhi->BeginRenderPass(m_GBuffer);
		rhi->BindPipeline(m_Pipelines["Phong Lighting"]);
		for (auto entity : entities_to_render)
		{
			Entity e = { entity, m_CurrentScene };
			std::shared_ptr<MeshSource> ms = e.GetComponent<MeshComponent>().mesh_source;
			if (!ms->has_mesh)
				continue;
			TransformComponent tc = e.GetComponent<TransformComponent>();
		
			rhi->BindVertexBuffer(ms->mesh_vb);
			rhi->BindIndexBuffer(ms->mesh_ib);
			for (auto& mesh : ms->meshes)
			{
				auto transform = tc.GetTransform();
				if (mesh->gltf_matrix != glm::mat4(1.0f))
					transform *= mesh->gltf_matrix;

				rhi->BindParameter("SceneData", m_SceneDataCB);
				rhi->BindParameter("Transform", &transform, sizeof(glm::mat4));
				rhi->BindParameter("DirectionalLights", m_DirectionalLightsBuffer);
				rhi->BindParameter("PointLights", m_PointLightsBuffer);
		
				for (auto& submesh : mesh->submeshes)
				{
					rhi->BindParameter("g_textureDiffuse", submesh->diffuse_texture);
					rhi->BindParameter("g_textureEmissive", submesh->emissive_texture);
					rhi->DrawIndexed(submesh->index_count, 1, submesh->index_start);
				}
			}
		}
		
		// Skybox
		if (m_SkyboxEnable)
		{
			rhi->BindPipeline(m_Pipelines["Skybox"]);
			m_Skybox->Render(rhi, m_ProjectionMatrix * glm::mat4(glm::mat3(m_ViewMatrix)));
		}
		rhi->EndRenderPass(m_GBuffer);

		// Scene Composite
		rhi->BeginRenderPass(m_SceneComposite);
		rhi->BindPipeline(m_Pipelines["Scene Composite"]);
		rhi->BindVertexBuffer(m_QuadBuffer);
		rhi->BindParameter("g_sceneTexture", m_GBuffer->color_attachments[0]);
		rhi->UpdateBufferData(m_SceneSettingsBuffer, &m_SceneSettings);
		rhi->BindParameter("SceneSettings", m_SceneSettingsBuffer);
		rhi->Draw(6);
		rhi->EndRenderPass(m_SceneComposite);

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