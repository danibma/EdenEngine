#include "Core/Window.h"
#include "Core/Memory.h"
#include "Core/Input.h"
#include "Core/Log.h"
#include "Core/Application.h"
#include "UI/UI.h"
#include "Core/Camera.h"
#include "Profiling/Timer.h"
#include "Profiling/Profiler.h"
#include "Graphics/D3D12RHI.h"
#include "Scene/MeshSource.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Scene/SceneSerializer.h"
#include "Math/Math.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>

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
	Buffer directional_lights_buffer;
	static const int MAX_POINT_LIGHTS = 32;
	Buffer point_light_components_buffer;

	std::unordered_map<const char*, Pipeline> m_Pipelines;

	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;
	SceneData scene_data;
	Buffer scene_data_cb;
	Camera camera;

	Entity m_Helmet;
	Entity flight_helmet;
	Entity m_BasicMesh;
	Entity m_FloorMesh;
	Entity m_DirectionalLight;

	// Skybox
	MeshSource m_SkyboxCube;
	Texture2D skybox_texture;
	struct SkyboxData
	{
		glm::mat4 view_projection_;
	} skybox_data;
	Buffer skybox_data_cb;

	bool skybox_enable = false;

	// UI
	bool m_OpenDebugWindow = true;
	bool m_OpenEntityProperties = true;
	bool m_ShowUI = true;
	bool m_CloseApp = false;
	bool m_OpenSceneHierarchy = true;
	bool m_OpenSceneProperties = false;
	bool m_OpenShadersPanel = false;
	int m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;

	// Scene
	Scene* m_CurrentScene = nullptr;
	Entity m_SelectedEntity;
	std::filesystem::path m_CurrentScenePath = "";
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

		camera = Camera(window->GetWidth(), window->GetHeight());

		view = glm::lookAtLH(camera.position, camera.position + camera.front, camera.up);
		projection = glm::perspectiveFovLH_ZO(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 200.0f);
		model = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
		scene_data.view = view;
		scene_data.view_projection = projection * view;
		scene_data.view_position = glm::vec4(camera.position, 1.0f);

		scene_data_cb = rhi->CreateBuffer<SceneData>(&scene_data, 1);

		skybox_data.view_projection_ = projection * glm::mat4(glm::mat3(view));
		skybox_data_cb = rhi->CreateBuffer<SkyboxData>(&skybox_data, 1);

		m_CurrentScene = enew Scene();
		std::string default_scene = "assets/scenes/demo.escene";
		OpenScene(default_scene);
		m_SkyboxCube.LoadGLTF(rhi, "assets/models/basic/cube.glb"); // TODO(Daniel): Create skybox class

		// Lights
		directional_lights_buffer = rhi->CreateBuffer<DirectionalLightComponent>(nullptr, MAX_DIRECTIONAL_LIGHTS, D3D12RHI::BufferType::kCreateSRV);
		UpdateDirectionalLights();
		point_light_components_buffer = rhi->CreateBuffer<PointLightComponent>(nullptr, MAX_POINT_LIGHTS, D3D12RHI::BufferType::kCreateSRV);
		UpdatePointLights();
	}

	void NewScene()
	{
		edelete m_CurrentScene;
		m_CurrentScene = enew Scene();
		ChangeWindowTitle(m_CurrentScene->GetName());
	}

	void OpenScene(const std::filesystem::path& path)
	{
		edelete m_CurrentScene;
		m_CurrentScene = enew Scene();
		SceneSerializer serializer(m_CurrentScene);
		serializer.Deserialize(path);
		m_CurrentScenePath = path;

		ChangeWindowTitle(m_CurrentScene->GetName());

		auto entities = m_CurrentScene->GetAllEntitiesWith<MeshComponent>();
		for (auto& entity : entities)
		{
			Entity e = { entity, m_CurrentScene };
			e.GetComponent<MeshComponent>().LoadMeshSource(rhi);
		}
	}

	void OpenSceneDialog()
	{
		std::string path = OpenFileDialog("Eden Scene (.escene)\0*.escene;\0");
		if (!path.empty())
			OpenScene(path);
	}

	void SaveScene()
	{
		ED_LOG_INFO("Saved scene: {}", m_CurrentScenePath);
		SceneSerializer serializer(m_CurrentScene);
		serializer.Serialize(m_CurrentScenePath);
	}

	void SaveSceneAs()
	{
		std::string path = SaveFileDialog("Eden Scene (.escene)\0*.escene;\0");
		if (!path.empty())
		{
			SceneSerializer serializer(m_CurrentScene);
			serializer.Serialize(path);
		}
	}

	void EntityMenu()
	{
		if (ImGui::MenuItem("Create Empty Entity"))
		{
			m_CurrentScene->CreateEntity();

			ImGui::CloseCurrentPopup();
		}

		if (ImGui::BeginMenu("3D Object"))
		{
			if (ImGui::MenuItem("Cube"))
			{
				auto e = m_CurrentScene->CreateEntity("Cube");
				e.AddComponent<MeshComponent>().LoadMeshSource(rhi, "assets/models/basic/cube.glb");

				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Plane"))
			{
				auto e = m_CurrentScene->CreateEntity("Plane");
				e.AddComponent<MeshComponent>().LoadMeshSource(rhi, "assets/models/basic/plane.glb");

				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Sphere"))
			{
				auto e = m_CurrentScene->CreateEntity("Sphere");
				e.AddComponent<MeshComponent>().LoadMeshSource(rhi, "assets/models/basic/sphere.glb");

				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Cone"))
			{
				auto e = m_CurrentScene->CreateEntity("Cone");
				e.AddComponent<MeshComponent>().LoadMeshSource(rhi, "assets/models/basic/cone.glb");

				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Cylinder"))
			{
				auto e = m_CurrentScene->CreateEntity("Cylinder");
				e.AddComponent<MeshComponent>().LoadMeshSource(rhi, "assets/models/basic/cylinder.glb");

				ImGui::CloseCurrentPopup();
			}

			ImGui::EndMenu();
		}
	}

	void UI_Toolbar()
	{
		const float toolbar_size = 1.0f;
		bool show_help_popup = false;

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y));
		ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, toolbar_size));
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags window_flags = 0
			| ImGuiWindowFlags_NoDocking
			| ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoScrollbar
			| ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_MenuBar
			;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
		ImGui::Begin("TOOLBAR", NULL, window_flags);
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();

		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.200f, 0.220f, 0.270f, 1.0f));
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
				ImGui::MenuItem("Entity Properties", NULL, &m_OpenEntityProperties);
				ImGui::MenuItem("Scene Hierarchy", NULL, &m_OpenSceneHierarchy);
				ImGui::MenuItem("Scene Properties", NULL, &m_OpenSceneProperties);
				ImGui::MenuItem("Shaders Panel", NULL, &m_OpenShadersPanel);

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Entity"))
			{
				EntityMenu();
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

			if (ImGui::BeginMenu("Help"))
			{
				if (ImGui::MenuItem("Help"))
					show_help_popup = !show_help_popup;

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		ImGui::PopStyleColor();

		// Help Popup
		if (show_help_popup)
			ImGui::OpenPopup("help_popup");
		if (ImGui::BeginPopupModal("help_popup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove))
		{
			UI::CenterWindow();
			ImGui::Dummy(ImVec2(0, 10));
			const char* string = "Press F3 to Hide the UI";
			UI::AlignedText(string, UI::Align::Center);
			ImGui::Dummy(ImVec2(0, 20));
			if (UI::AlignedButton("Close", UI::Align::Center))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		ImGui::End();
	}

	void UI_SceneHierarchy()
	{
		ImGui::Begin("Scene Hierarchy", &m_OpenSceneHierarchy, ImGuiWindowFlags_NoCollapse);
		auto& entities = m_CurrentScene->GetAllEntitiesWith<TagComponent>();
		for (int i = entities.size() - 1; i >= 0; --i)
		{
			entt::entity entity = entities[i];
			Entity e = { entity, m_CurrentScene };
			auto& tag = e.GetComponent<TagComponent>();
			if (tag.tag.length() == 0) tag.tag = " "; // If the tag is empty add a space to it doesnt crash
			if (ImGui::Selectable(tag.tag.c_str(), m_SelectedEntity == e))
				m_SelectedEntity = e;

			if (ImGui::BeginPopupContextItem())
			{
				m_SelectedEntity = e;
				if (ImGui::MenuItem("Delete", "Del"))
				{
					m_CurrentScene->DeleteEntity(m_SelectedEntity);
				}
				ImGui::Separator();
				EntityMenu();

				ImGui::EndPopup();
			}
		}

		// Scene hierarchy popup to create new entities
		if (Input::GetMouseButton(ED_MOUSE_RIGHT) && ImGui::IsWindowHovered())
			ImGui::OpenPopup("hierarchy_popup");

		if (ImGui::BeginPopup("hierarchy_popup"))
		{
			EntityMenu();
			ImGui::EndPopup();
		}

		if (Input::GetKeyDown(ED_KEY_DELETE) && ImGui::IsWindowFocused())
			m_CurrentScene->DeleteEntity(m_SelectedEntity);

		ImGui::End();
	}

	template <typename T>
	void DrawComponentProperty(const char* title, std::function<void()> func)
	{
		if (m_SelectedEntity.HasComponent<T>())
		{
			ImGui::PushID((void*)typeid(T).hash_code());

			bool open = ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap);
			ImGui::SameLine(ImGui::GetWindowWidth() - 25.0f);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			if (ImGui::Button("...", ImVec2(20, 19)))
			{
				ImGui::OpenPopup("component_settings");
			}
			ImGui::PopStyleColor();

			if (ImGui::BeginPopup("component_settings"))
			{
				bool remove_component = true;
				if (std::is_same<T, TransformComponent>::value)
					remove_component = false;

				if (ImGui::MenuItem("Remove Component", nullptr, false, remove_component))
				{
					m_SelectedEntity.RemoveComponent<T>();
					open = false;

					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}
			ImGui::PopID();

			if (open)
				func();

		}
	}

	void UI_EntityProperties()
	{
		ImGui::Begin("Entity Properties", &m_OpenEntityProperties, ImGuiWindowFlags_NoCollapse);
		if (!m_SelectedEntity) goto end;

		if (m_SelectedEntity.HasComponent<TagComponent>())
		{
			auto& tag = m_SelectedEntity.GetComponent<TagComponent>().tag;
			constexpr size_t num_chars = 256;
			char buffer[num_chars];
			strcpy_s(buffer, tag.c_str());
			ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
			if (ImGui::InputText("##Tag", buffer, num_chars))
				tag = std::string(buffer);
			ImGui::PopItemWidth();
		}
		ImGui::SameLine();
		{
			if (UI::AlignedButton("Add Component", UI::Align::Right))
				ImGui::OpenPopup("addc_popup");

			if (ImGui::BeginPopup("addc_popup"))
			{
				bool mesh_component = !m_SelectedEntity.HasComponent<MeshComponent>();
				bool point_light_component = !m_SelectedEntity.HasComponent<PointLightComponent>();
				bool directional_light_component = !m_SelectedEntity.HasComponent<DirectionalLightComponent>();

				if (ImGui::MenuItem("Mesh Component", 0, false, mesh_component))
				{
					m_SelectedEntity.AddComponent<MeshComponent>();
					ImGui::CloseCurrentPopup();
				}

				if (ImGui::MenuItem("Point Light Component", 0, false, point_light_component))
				{
					m_SelectedEntity.AddComponent<PointLightComponent>();
					ImGui::CloseCurrentPopup();
				}

				if (ImGui::MenuItem("Directional Light Component", 0, false, directional_light_component))
				{
					m_SelectedEntity.AddComponent<DirectionalLightComponent>();
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}
		}

		DrawComponentProperty<TransformComponent>("Transform", [&]() {
			auto& transform = m_SelectedEntity.GetComponent<TransformComponent>();
			UI::DrawVec3("Translation", transform.translation);
			glm::vec3 rotation = glm::degrees(transform.rotation);
			UI::DrawVec3("Rotation", rotation);
			transform.rotation = glm::radians(rotation);
			UI::DrawVec3("Scale", transform.scale, 1.0f);
		});

		ImGui::Spacing();
		ImGui::Spacing();

		DrawComponentProperty<MeshComponent>("Mesh", [&]() {
			auto& mc = m_SelectedEntity.GetComponent<MeshComponent>();

			ImGui::Text("File Path");
			ImGui::SameLine();
			constexpr size_t num_chars = 256;
			char buffer[num_chars];
			memset(buffer, 0, num_chars);
			memcpy(buffer, mc.mesh_path.c_str(), mc.mesh_path.length());
			ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.9f);
			ImGui::InputText("###meshpath", buffer, num_chars, ImGuiInputTextFlags_ReadOnly);
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (UI::AlignedButton("...", UI::Align::Right))
			{
				std::string new_path = OpenFileDialog("Model formats (.gltf, .glb)\0*.gltf;*.glb\0");
				if (!new_path.empty())
					mc.LoadMeshSource(rhi, new_path);
			}
		});

		ImGui::Spacing();
		ImGui::Spacing();

		DrawComponentProperty<PointLightComponent>("Point Light", [&]() {
			auto& pl = m_SelectedEntity.GetComponent<PointLightComponent>();
			auto& transform = m_SelectedEntity.GetComponent<TransformComponent>();
			
			pl.position = glm::vec4(transform.translation, 1.0f);

			UI::DrawColor("Color", pl.color);
			UI::DrawProperty("Constant", pl.constant_value, 0.05f, 0.0f, 1.0f);
			UI::DrawProperty("Linear", pl.linear_value, 0.05f, 0.0f, 1.0f);
			UI::DrawProperty("Quadratic", pl.quadratic_value, 0.05f, 0.0f, 1.0f);
		});

		ImGui::Spacing();
		ImGui::Spacing();

		DrawComponentProperty<DirectionalLightComponent>("Directional Light", [&]() {
			auto& dl = m_SelectedEntity.GetComponent<DirectionalLightComponent>();
			auto& transform = m_SelectedEntity.GetComponent<TransformComponent>();

			dl.direction = glm::vec4(transform.rotation, 1.0f);
			
			UI::DrawProperty("Intensity", dl.intensity, 0.05f, 0.0f, 1.0f);
		});

		end: // goto end
		ImGui::End();
	}

	void UI_SceneProperties()
	{
		ImGui::Begin("Scene Properties", &m_OpenSceneProperties, ImGuiWindowFlags_NoCollapse);
		ImGui::Checkbox("Enable Skybox", &skybox_enable);
		ImGui::End();
	}

	void UI_DebugWindow()
	{
		ImGui::Begin("Debug", &m_OpenDebugWindow, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

		ImGui::Text("CPU frame time: %.3fms(%.1fFPS)", delta_time * 1000.0f, (1000.0f / delta_time) / 1000.0f);

		ImGui::End();
	}

	void UI_DrawGuizmo()
	{
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();

		float window_width = window->GetWidth();
		float window_height = window->GetHeight();
		ImGuizmo::SetRect(0.0f, 0.0f, window_width, window_height);

		auto& transform_component = m_SelectedEntity.GetComponent<TransformComponent>();
		glm::mat4 transform = transform_component.GetTransform();

		ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), static_cast<ImGuizmo::OPERATION>(m_GizmoType), ImGuizmo::LOCAL, glm::value_ptr(transform));

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
			ImGui::PushID(pipeline.second.name.c_str());
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

	void UI_Render()
	{
		// ImGui::ShowDemoWindow(nullptr);

		UI_Toolbar();
		if (m_OpenDebugWindow)
			UI_DebugWindow();
		if (m_OpenEntityProperties)
			UI_EntityProperties();
		if (m_OpenSceneHierarchy)
			UI_SceneHierarchy();
		if (m_OpenSceneProperties)
			UI_SceneProperties();
		if (m_SelectedEntity)
			UI_DrawGuizmo();
		if (m_OpenShadersPanel)
			UI_PipelinesPanel();
	}

	void EditorInput()
	{
		// BUG: if we are writing inside a textbox it will switch the gizmos too
		//		find a way to detect if any window is focused, if there is any, dont do these actions
		if (!Input::GetMouseButton(MouseButton::RightButton))
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

		// Show UI or not
		if (Input::GetKeyDown(KeyCode::F3))
			m_ShowUI = !m_ShowUI;
		if (m_ShowUI)
			UI_Render();

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

		rhi->UpdateBuffer<PointLightComponent>(point_light_components_buffer, point_light_components.data(), point_light_components.size());
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

		rhi->UpdateBuffer<DirectionalLightComponent>(directional_lights_buffer, directional_light_components.data(), directional_light_components.size());
	}

	void OnUpdate() override
	{
		ED_PROFILE_FUNCTION();

		rhi->BeginRender();

		// Update camera and scene data
		camera.Update(window, delta_time);
		view = glm::lookAtLH(camera.position, camera.position + camera.front, camera.up);
		projection = glm::perspectiveFovLH_ZO(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 2000.0f);
		scene_data.view = view;
		scene_data.view_projection = projection * view;
		scene_data.view_position = glm::vec4(camera.position, 1.0f);
		rhi->UpdateBuffer<SceneData>(scene_data_cb, &scene_data, 1);

		EditorInput();

		rhi->ClearRenderTargets();

		UpdateDirectionalLights();
		UpdatePointLights();

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

				rhi->BindParameter("SceneData", scene_data_cb);
				rhi->BindParameter("Transform", ms->transform_cb);
				rhi->BindParameter("DirectionalLights", directional_lights_buffer);
				rhi->BindParameter("PointLights", point_light_components_buffer);

				for (auto& submesh : mesh.submeshes)
				{
					if (textured)
					{
						rhi->BindParameter("g_textureDiffuse", submesh.diffuse_texture);
						rhi->BindParameter("g_textureEmissive", submesh.emissive_texture);
					}
					rhi->DrawIndexed(submesh.index_count, 1, submesh.index_start);
				}
			}
		}

		// Skybox
		if (skybox_enable)
		{
			// this should not be loaded during runtime, but yeah...
			if (!skybox_texture.resource) // If the texture is not loaded yet, load it
			{
				skybox_texture = rhi->CreateTexture2D("assets/skyboxes/sky.hdr");
				skybox_texture.resource->SetName(L"Skybox texture");
			}

			rhi->BindPipeline(m_Pipelines["Skybox"]);
			rhi->BindVertexBuffer(m_SkyboxCube.mesh_vb);
			rhi->BindIndexBuffer(m_SkyboxCube.mesh_ib);
			for (auto& mesh : m_SkyboxCube.meshes)
			{
				skybox_data.view_projection_ = projection * glm::mat4(glm::mat3(view));
				rhi->UpdateBuffer<SkyboxData>(skybox_data_cb, &skybox_data, 1);
				rhi->BindParameter("SkyboxData", skybox_data_cb);

				for (auto& submesh : mesh.submeshes)
				{
					rhi->BindParameter("g_cubemapTexture", skybox_texture);
					rhi->DrawIndexed(submesh.index_count, 1, submesh.index_start);
				}
			}
		}

		rhi->EndRender();
		rhi->Render();
	}


	void OnDestroy() override
	{
		auto entities_with_mesh = m_CurrentScene->GetAllEntitiesWith<MeshComponent>();
		for (auto entity : entities_with_mesh)
		{
			Entity e = { entity, m_CurrentScene };
			e.GetComponent<MeshComponent>().mesh_source->Destroy();
		}

		edelete m_CurrentScene;

		point_light_components_buffer.Release();
		directional_lights_buffer.Release();
		scene_data_cb.Release();
		skybox_data_cb.Release();
		m_SkyboxCube.Destroy();
		skybox_texture.Release();
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