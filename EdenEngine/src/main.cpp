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
#include "Scene/LightCasters.h"
#include "Scene/MeshSource.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Math/Math.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

	DirectionalLight directional_light;
	Buffer directional_light_cb;
	float directional_light_intensity = 0.5f;
	std::vector<PointLight> point_lights;
	Buffer point_lights_cb;

	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;
	glm::vec3 light_direction = glm::vec3(0.0f, 10.0f, 0.0f);
	SceneData scene_data;
	Buffer scene_data_cb;
	Camera camera;

	Pipeline object_texture;
	Pipeline object_simple;
	Pipeline light_caster;
	Entity m_Helmet;
	Entity flight_helmet;
	Entity m_BasicMesh;
	Entity m_FloorMesh;

	// Skybox
	Pipeline skybox;
	MeshSource m_SkyboxCube; // Temp: In the future create a skybox class
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
	int m_GuizmoType = ImGuizmo::OPERATION::TRANSLATE;

	// Scene
	Scene* m_CurrentScene = nullptr;
	Entity m_SelectedEntity;
public:
	EdenApplication() = default;

	void OnInit() override
	{
		PipelineState object_textureDS;
		object_textureDS.enable_blending = true;
		object_texture = rhi->CreateGraphicsPipeline("object_texture", object_textureDS);
		object_simple = rhi->CreateGraphicsPipeline("object_simple");
		light_caster = rhi->CreateGraphicsPipeline("light_caster");

		PipelineState skybox_ds;
		skybox_ds.cull_mode = D3D12_CULL_MODE_NONE;
		skybox_ds.depth_func = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		skybox_ds.min_depth = 1.0f;
		skybox = rhi->CreateGraphicsPipeline("skybox", skybox_ds);

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
		m_Helmet = m_CurrentScene->CreateEntity("Helmet");
		m_Helmet.GetComponent<TransformComponent>().translation = glm::vec3(-3, 0, 0);
		m_BasicMesh = m_CurrentScene->CreateEntity("Sphere");
		m_BasicMesh.GetComponent<TransformComponent>().translation = glm::vec3(3, 0, 0);
		m_FloorMesh = m_CurrentScene->CreateEntity("Floor");
		m_FloorMesh.GetComponent<TransformComponent>().translation = glm::vec3(0, -2, 0);
		m_FloorMesh.GetComponent<TransformComponent>().scale = glm::vec3(10.0f, 0.2f, 10.0f);
		m_Helmet.AddComponent<MeshComponent>().LoadMeshSource(rhi, "assets/Models/DamagedHelmet/DamagedHelmet.glb");
		m_BasicMesh.AddComponent<MeshComponent>().LoadMeshSource(rhi, "assets/Models/basic/sphere.glb");
		m_FloorMesh.AddComponent<MeshComponent>().LoadMeshSource(rhi, "assets/Models/basic/cube.glb");

		m_SkyboxCube.LoadGLTF(rhi, "assets/Models/basic/cube.glb");

		// Lights
		directional_light.data.direction = glm::vec4(light_direction, 1.0f);
		directional_light.data.intensity = directional_light_intensity;
		directional_light_cb = rhi->CreateBuffer<DirectionalLight::DirectionalLightData>(&directional_light.data, 1);

		point_lights.emplace_back();
		point_lights.back().model.LoadGLTF(rhi, "assets/Models/basic/cube.glb");
		point_lights.back().data.position = glm::vec4(3.0f, 3.0f, -5.0f, 1.0f);
		point_lights.back().data.color = glm::vec4(0.0f, 1.0f, 1.0f, 0.0f);
		point_lights.back().light_color_buffer = rhi->CreateBuffer<glm::vec4>(&point_lights.back().data.color, 1);

		point_lights.emplace_back();
		point_lights.back().model.LoadGLTF(rhi, "assets/Models/basic/cube.glb");
		point_lights.back().data.position = glm::vec4(-3.0f, 3.0f, -5.0f, 1.0f);
		point_lights.back().data.color = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
		point_lights.back().light_color_buffer = rhi->CreateBuffer<glm::vec4>(&point_lights.back().data.color, 1);

		std::vector<PointLight::PointLightData> point_lights_data(point_lights.size());
		point_lights_data.emplace_back(point_lights[0].data);
		point_lights_data.emplace_back(point_lights[1].data);
		point_lights_cb = rhi->CreateBuffer<PointLight::PointLightData>(point_lights_data.data(), point_lights_data.size(), D3D12RHI::BufferType::kCreateSRV);
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
				e.AddComponent<MeshComponent>().LoadMeshSource(rhi, "assets/Models/basic/cube.glb");

				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Plane"))
			{
				auto e = m_CurrentScene->CreateEntity("Plane");
				e.AddComponent<MeshComponent>().LoadMeshSource(rhi, "assets/Models/basic/plane.glb");

				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Sphere"))
			{
				auto e = m_CurrentScene->CreateEntity("Sphere");
				e.AddComponent<MeshComponent>().LoadMeshSource(rhi, "assets/Models/basic/sphere.glb");

				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Cone"))
			{
				auto e = m_CurrentScene->CreateEntity("Cone");
				e.AddComponent<MeshComponent>().LoadMeshSource(rhi, "assets/Models/basic/cone.glb");

				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Cylinder"))
			{
				auto e = m_CurrentScene->CreateEntity("Cylinder");
				e.AddComponent<MeshComponent>().LoadMeshSource(rhi, "assets/Models/basic/cylinder.glb");

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
			if (ImGui::Selectable(tag.tag.c_str(), m_SelectedEntity == e))
				m_SelectedEntity = e;
		}

		// Scene hierarchy popup to create new entities
		if (Input::GetMouseButton(ED_MOUSE_RIGHT) && ImGui::IsWindowHovered())
			ImGui::OpenPopup("hierarchy_popup");

		if (ImGui::BeginPopup("hierarchy_popup"))
		{
			EntityMenu();
			ImGui::EndPopup();
		}


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
					if (std::is_same<T, MeshComponent>::value)
						m_SelectedEntity.GetComponent<MeshComponent>().mesh_source->Destroy();
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
			memset(buffer, 0, num_chars);
			memcpy(buffer, tag.c_str(), tag.length());
			ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.65f);
			if (ImGui::InputText("##Tag", buffer, num_chars))
			{
				tag = std::string(buffer);
			}
			ImGui::PopItemWidth();
		}
		ImGui::SameLine();
		{
			if (UI::AlignedButton("Add Component", UI::Align::Right))
				ImGui::OpenPopup("addc_popup");

			if (ImGui::BeginPopup("addc_popup"))
			{
				bool mesh_component = !m_SelectedEntity.HasComponent<MeshComponent>();

				if (ImGui::MenuItem("Mesh Component", 0, false, mesh_component))
				{
					m_SelectedEntity.AddComponent<MeshComponent>();
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
		
		end: // goto end
		ImGui::End();
	}

	void UI_SceneProperties()
	{
		// TEMP
		ImGui::Begin("Scene Properties", &m_OpenSceneProperties, ImGuiWindowFlags_NoCollapse);
		ImGui::DragFloat3("Direction", (float*)&light_direction, 0.10f, 0.0f, 0.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("Intensity", &directional_light_intensity, 0.1f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::Checkbox("Enable Skybox", &skybox_enable);
		ImGui::End();
	}

	void UI_DebugWindow()
	{
		ImGui::Begin("Debug", &m_OpenDebugWindow, ImGuiWindowFlags_NoCollapse);

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

		ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), static_cast<ImGuizmo::OPERATION>(m_GuizmoType), ImGuizmo::LOCAL, glm::value_ptr(transform));

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

	void UI_Render()
	{
		//ImGui::ShowDemoWindow(nullptr);

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
	}

	void EditorInput()
	{
		if (!Input::GetMouseButton(MouseButton::RightButton))
		{
			if (Input::GetKey(KeyCode::Q))
				m_GuizmoType = -1;
			if (Input::GetKey(KeyCode::W))
				m_GuizmoType = ImGuizmo::TRANSLATE;
			if (Input::GetKey(KeyCode::E))
				m_GuizmoType = ImGuizmo::ROTATE;
			if (Input::GetKey(KeyCode::R))
				m_GuizmoType = ImGuizmo::SCALE;
		}

		// Show UI or not
		if (Input::GetKeyDown(KeyCode::F3))
			m_ShowUI = !m_ShowUI;
		if (m_ShowUI)
			UI_Render();

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

		// Update directional light
		directional_light.data.direction = glm::vec4(light_direction, 1.0f);
		directional_light.data.intensity = directional_light_intensity;
		rhi->UpdateBuffer<DirectionalLight>(directional_light_cb, &directional_light.data, 1);

		EditorInput();

		rhi->ClearRenderTargets();

		auto entities_to_render = m_CurrentScene->GetAllEntitiesWith<MeshComponent>();
		for (auto entity : entities_to_render)
		{
			Entity e = { entity, m_CurrentScene };
			std::shared_ptr<MeshSource> ms = e.GetComponent<MeshComponent>().mesh_source;
			if (!ms->transform_cb.resource) continue;
			TransformComponent tc = e.GetComponent<TransformComponent>();
			bool textured = ms->textures.size() > 0;
			if (textured)
				rhi->BindPipeline(object_texture);
			else
				rhi->BindPipeline(object_simple);

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
				rhi->BindParameter("DirectionalLightCB", directional_light_cb);
				rhi->BindParameter("PointLights", point_lights_cb);

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

	#if 0
		// Draw point lights
		rhi->BindPipeline(light_caster);
		for (auto& point_light : point_lights)
		{
			rhi->BindVertexBuffer(point_light.model.mesh_vb);
			rhi->BindIndexBuffer(point_light.model.mesh_ib);

			for (auto& mesh : point_light.model.meshes)
			{
				mesh.SetTranslation(point_light.data.position.x, point_light.data.position.y, point_light.data.position.z);
				mesh.SetScale(0.2f, 0.2f, 0.2f);
				mesh.UpdateTransform();
				rhi->UpdateBuffer<glm::mat4>(mesh.transform_cb, &mesh.transform, 1);

				scene_data.view = view;
				scene_data.view_projection = projection * view;
				scene_data.view_position = glm::vec4(camera.position, 1.0f);

				directional_light.data.direction = glm::vec4(light_direction, 1.0f);
				directional_light.data.intensity = directional_light_intensity;
				rhi->UpdateBuffer<DirectionalLight>(directional_light_cb, &directional_light.data, 1);

				rhi->UpdateBuffer<SceneData>(scene_data_cb, &scene_data, 1);
				rhi->BindConstantBuffer("SceneData", scene_data_cb);
				rhi->BindConstantBuffer("Transform", mesh.transform_cb);
				rhi->BindConstantBuffer("LightColor", point_light.light_color_buffer);

				for (auto& submesh : mesh.submeshes)
					rhi->DrawIndexed(submesh.index_count, 1, submesh.index_start);
			}
		}
	#endif

		// Skybox
		if (skybox_enable)
		{
			// this should not be loaded during runtime, but yeah...
			if (!skybox_texture.resource) // If the texture is not loaded yet, load it
			{
				skybox_texture = rhi->CreateTexture2D("assets/skyboxes/sky.hdr");
				skybox_texture.resource->SetName(L"Skybox texture");
			}

			rhi->BindPipeline(skybox);
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

		point_lights_cb.Release();
		directional_light_cb.Release();
		scene_data_cb.Release();
		skybox_data_cb.Release();
		m_SkyboxCube.Destroy();
		skybox_texture.Release();

		for (auto& point_light : point_lights)
			point_light.Destroy();
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

