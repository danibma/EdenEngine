#include "Core/Window.h"
#include "Core/Memory.h"
#include "Core/Input.h"
#include "Core/Log.h"
#include "Core/Application.h"
#include "UI/UI.h"
#include "Core/Camera.h"
#include "Profiling/Timer.h"
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

	static const int MAX_DIRECTIONAL_LIGHTS = 16;
	std::shared_ptr<Buffer> m_DirectionalLightsBuffer;
	static const int MAX_POINT_LIGHTS = 32;
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
	bool m_OpenDebugWindow = true;
	bool m_OpenSceneProperties = true;
	bool m_OpenShadersPanel = false;
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
public:
	EdenApplication() = default;

	void OnInit() override
	{
		PipelineState object_textureDS;
		object_textureDS.enable_blending = true;
		m_Pipelines["Object Texture"] = rhi->CreateGraphicsPipeline("object_texture", object_textureDS);
		m_Pipelines["Object Simple"] = rhi->CreateGraphicsPipeline("object_simple");

		PipelineState skybox_ds;
		skybox_ds.cull_mode = D3D12_CULL_MODE_NONE;
		skybox_ds.depth_func = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		skybox_ds.min_depth = 1.0f;
		m_Pipelines["Skybox"] = rhi->CreateGraphicsPipeline("skybox", skybox_ds);

		m_Camera = Camera(window->GetWidth(), window->GetHeight());

		m_ViewMatrix = glm::lookAtLH(m_Camera.position, m_Camera.position + m_Camera.front, m_Camera.up);
		m_ProjectionMatrix = glm::perspectiveFovLH_ZO(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 200.0f);
		m_SceneData.view = m_ViewMatrix;
		m_SceneData.view_projection = m_ProjectionMatrix * m_ViewMatrix;
		m_SceneData.view_position = glm::vec4(m_Camera.position, 1.0f);
		m_SceneDataCB = rhi->CreateBuffer<SceneData>(&m_SceneData, 1);

		m_CurrentScene = enew Scene();
		std::string default_scene = "assets/scenes/demo.escene";
		OpenScene(default_scene);

		m_Skybox = std::make_shared<Skybox>(rhi, "assets/skyboxes/studio_garden.hdr");

		// Lights
		m_DirectionalLightsBuffer = rhi->CreateBuffer<DirectionalLightComponent>(nullptr, MAX_DIRECTIONAL_LIGHTS, D3D12RHI::BufferType::kCreateSRV);
		UpdateDirectionalLights();
		m_PointLightsBuffer = rhi->CreateBuffer<PointLightComponent>(nullptr, MAX_POINT_LIGHTS, D3D12RHI::BufferType::kCreateSRV);
		UpdatePointLights();

		// Rendering
	#if WITH_EDITOR
		m_GBuffer = rhi->CreateRenderPass(RenderPass::Type::kRenderTexture, L"GBuffer_");
		m_ImGuiPass = rhi->CreateRenderPass(RenderPass::Type::kRenderTarget, L"ImGuiPass_", true);
		rhi->SetRTRenderPass(m_ImGuiPass);
		rhi->EnableImGui();

		m_ContentBrowserPanel = std::make_unique<ContentBrowserPanel>(rhi);
		m_SceneHierarchy = std::make_unique<SceneHierarchy>(rhi, m_CurrentScene);
	#else
		m_GBuffer = rhi->CreateRenderPass(RenderPass::Type::kRenderTarget, L"GBuffer_");
		rhi->SetRTRenderPass(&m_GBuffer);
	#endif
		m_ViewportSize = { window->GetWidth(), window->GetHeight() };
		m_ViewportPos = { 0, 0 };
	}

	void PrepareScene()
	{
		if (m_CurrentScene->IsSceneLoaded())
		{
			m_CurrentScene->ExecutePreparations();
			return;
		}

		auto scene_to_load = m_CurrentScene->GetScenePath();

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
				ImGui::MenuItem("Debug", NULL, &m_OpenDebugWindow);
				ImGui::MenuItem("Entity Properties", NULL, &m_SceneHierarchy->open_entity_properties);
				ImGui::MenuItem("Scene Hierarchy", NULL, &m_SceneHierarchy->open_scene_hierarchy);
				ImGui::MenuItem("Scene Properties", NULL, &m_OpenSceneProperties);
				ImGui::MenuItem("Shaders Panel", NULL, &m_OpenShadersPanel);
				ImGui::MenuItem("ContentBrowser", NULL, &m_ContentBrowserPanel->open_content_browser);

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
		ImGui::Begin("Scene Properties", &m_OpenSceneProperties, ImGuiWindowFlags_NoCollapse);
		ImGui::Checkbox("Enable Skybox", &m_SkyboxEnable);
		ImGui::End();
	}

	void UI_DebugWindow()
	{
		ImGui::Begin("Debug", &m_OpenDebugWindow, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

		ImGui::Text("CPU frame time: %.3fms(%.1fFPS)", delta_time * 1000.0f, (1000.0f / delta_time) / 1000.0f);
		ImGui::Text("Viewport Size: %.0fx%.0f", m_ViewportSize.x, m_ViewportSize.y);

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
		ImGui::Begin("Pipelines");
		ImGui::Columns(2);
		ImGui::NextColumn();
		if (ImGui::Button("Reload all pipelines"))
		{
			for (auto& pipeline : m_Pipelines)
				rhi->ReloadPipeline(pipeline.second);
		}
		ImGui::Columns(1);
		for (auto& pipeline : m_Pipelines)
		{
			ImGui::PushID(pipeline.second->name.c_str());
			ImGui::Columns(2);
			ImGui::Text(pipeline.first);
			ImGui::NextColumn();
			if (ImGui::Button("Reload pipeline"))
				rhi->ReloadPipeline(pipeline.second);
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
			m_GBuffer->width = (uint32_t)m_ViewportSize.x;
			m_GBuffer->height = (uint32_t)m_ViewportSize.y;
			m_Camera.SetViewportSize(m_ViewportSize);
		}
		auto viewport_pos = ImGui::GetWindowPos();
		m_ViewportPos = { (uint32_t)viewport_pos.x, (uint32_t)viewport_pos.y };
		m_Camera.SetViewportPosition(m_ViewportPos);

		m_ViewportFocused = ImGui::IsWindowFocused();

		ImGui::Image((ImTextureID)m_GBuffer->render_targets[0]->gpu_handle.ptr, viewport_size);

		// Drag and drop
		if (ImGui::BeginDragDropTarget())
		{
			if (auto payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP))
			{
				std::filesystem::path path = std::filesystem::path(reinterpret_cast<const char*>(payload->Data));
				auto extension = path.extension().string();

				// TODO: Find a way to not do this manually, like iterate through the vector in some way, idk...
				if (extension == SceneSerializer::DefaultExtension)
				{
					OpenScene(path);
				}
				else if (extension == ".gltf" || extension == ".glb")
				{
					auto e = m_CurrentScene->CreateEntity(path.stem().string());
					auto& mc = e.AddComponent<MeshComponent>();
					mc.mesh_path = path.string();
					m_CurrentScene->AddPreparation([&]() {
						mc.LoadMeshSource(rhi);
					});
				} 
				else if (extension == ".hdr")
				{
					m_Skybox->SetNewTexture(path.string().c_str());
					m_CurrentScene->AddPreparation([&]() {
						m_Skybox->UpdateNewTexture(rhi);
					});
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void UI_Render()
	{
		//ImGui::ShowDemoWindow(nullptr);

		UI_Dockspace();
		UI_Viewport();
		if (m_OpenDebugWindow)
			UI_DebugWindow();
		if (m_OpenSceneProperties)
			UI_SceneProperties();
		if (m_CurrentScene->GetSelectedEntity())
			UI_DrawGizmo();
		if (m_OpenShadersPanel)
			UI_PipelinesPanel();
		m_ContentBrowserPanel->Render();
		m_SceneHierarchy->Render();
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

		rhi->UpdateBuffer<PointLightComponent>(m_PointLightsBuffer, point_light_components.data(), point_light_components.size());
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

		rhi->UpdateBuffer<DirectionalLightComponent>(m_DirectionalLightsBuffer, directional_light_components.data(), directional_light_components.size());
	}

	void OnUpdate() override
	{
		ED_PROFILE_FUNCTION();

		rhi->BeginRender();

	#if !WITH_EDITOR
		m_ViewportSize = { window->GetWidth(), window->GetHeight() };
	#endif

		// Update camera and scene data
		m_Camera.Update(delta_time);
		m_ViewMatrix = glm::lookAtLH(m_Camera.position, m_Camera.position + m_Camera.front, m_Camera.up);
		m_ProjectionMatrix = glm::perspectiveFovLH_ZO(glm::radians(70.0f), m_ViewportSize.x, m_ViewportSize.y, 0.1f, 2000.0f);
		m_SceneData.view = m_ViewMatrix;
		m_SceneData.view_projection = m_ProjectionMatrix * m_ViewMatrix;
		m_SceneData.view_position = glm::vec4(m_Camera.position, 1.0f);
		rhi->UpdateBuffer<SceneData>(m_SceneDataCB, &m_SceneData, 1);

		EditorInput();

		UpdateDirectionalLights();
		UpdatePointLights();
		PrepareScene();

		rhi->BeginRenderPass(m_GBuffer);
		auto entities_to_render = m_CurrentScene->GetAllEntitiesWith<MeshComponent>();
		for (auto entity : entities_to_render)
		{
			Entity e = { entity, m_CurrentScene };
			std::shared_ptr<MeshSource> ms = e.GetComponent<MeshComponent>().mesh_source;
			if (!ms->transform_cb) continue;
			TransformComponent tc = e.GetComponent<TransformComponent>();
			bool textured = ms->textures.size() > 0;
			if (textured)
				rhi->BindPipeline(m_Pipelines["Object Texture"]);
			else
				rhi->BindPipeline(m_Pipelines["Object Simple"]);

			rhi->UpdateBuffer<glm::mat4>(ms->transform_cb, &tc.GetTransform(), 1);

			rhi->BindVertexBuffer(ms->mesh_vb);
			rhi->BindIndexBuffer(ms->mesh_ib);
			for (auto& mesh : ms->meshes)
			{
				if (mesh.gltf_matrix != glm::mat4(1.0f))
				{
					auto& transform = tc.GetTransform();
					transform *= mesh.gltf_matrix;
					rhi->UpdateBuffer<glm::mat4>(ms->transform_cb, &transform, 1);
				}

				rhi->BindParameter("SceneData", m_SceneDataCB);
				rhi->BindParameter("Transform", ms->transform_cb);
				rhi->BindParameter("DirectionalLights", m_DirectionalLightsBuffer);
				rhi->BindParameter("PointLights", m_PointLightsBuffer);

				for (auto& submesh : mesh.submeshes)
				{
					if (textured)
					{
						if (submesh.diffuse_texture)
							rhi->BindParameter("g_textureDiffuse", submesh.diffuse_texture);
						if (submesh.emissive_texture)
							rhi->BindParameter("g_textureEmissive", submesh.emissive_texture);
					}
					rhi->DrawIndexed(submesh.index_count, 1, submesh.index_start);
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

	#if WITH_EDITOR
		rhi->BeginRenderPass(m_ImGuiPass);
		UI_Render();
		rhi->EndRenderPass(m_ImGuiPass);
	#endif

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