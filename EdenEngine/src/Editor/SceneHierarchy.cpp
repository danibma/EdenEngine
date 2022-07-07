#include "SceneHierarchy.h"

#include <imgui/imgui.h>

#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "UI/UI.h"
#include "Core/Input.h"
#include "Core/Application.h"

namespace Eden
{
	void SceneHierarchy::DrawHierarchy()
	{
		ImGui::Begin("Scene Hierarchy", &open_scene_hierarchy, ImGuiWindowFlags_NoCollapse);
		
		auto entities = m_CurrentScene->GetAllEntitiesWith<TagComponent>();

		// This loop is inversed because that way the last added entity shows at the bottom
		int num_entities = static_cast<int>(entities.size() - 1);
		for (int i = num_entities; i >= 0; --i)
		{

			Entity e = { entities[i], m_CurrentScene };
			ImGui::PushID(e.GetID());
			auto& tag = e.GetComponent<TagComponent>();
			if (tag.tag.length() == 0) tag.tag = " "; // If the tag is empty add a space to it doesnt crash
			if (ImGui::Selectable(tag.tag.c_str(), m_CurrentScene->GetSelectedEntity() == e))
				m_CurrentScene->SetSelectedEntity(e);

			if (ImGui::BeginPopupContextItem())
			{
				m_CurrentScene->SetSelectedEntity(e);
				if (ImGui::MenuItem("Delete", "Del"))
				{
					m_CurrentScene->AddPreparation([&]() {
						m_CurrentScene->DeleteEntity(m_CurrentScene->GetSelectedEntity());
					});
				}
				ImGui::Separator();
				EntityMenu();

				ImGui::EndPopup();
			}

			ImGui::PopID();
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
		{
			m_CurrentScene->AddPreparation([&]() {
				m_CurrentScene->DeleteEntity(m_CurrentScene->GetSelectedEntity());
			});
		}

		ImGui::End();
	}

	template <typename T>
	static void DrawComponentProperty(Entity selected_entity, const char* title, std::function<void()> func)
	{
		if (selected_entity.HasComponent<T>())
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
					selected_entity.RemoveComponent<T>();
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

	void SceneHierarchy::EntityMenu()
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
				e.AddComponent<MeshComponent>().LoadMeshSource(m_RHI, "assets/models/basic/cube.glb");

				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Plane"))
			{
				auto e = m_CurrentScene->CreateEntity("Plane");
				e.AddComponent<MeshComponent>().LoadMeshSource(m_RHI, "assets/models/basic/plane.glb");

				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Sphere"))
			{
				auto e = m_CurrentScene->CreateEntity("Sphere");
				e.AddComponent<MeshComponent>().LoadMeshSource(m_RHI, "assets/models/basic/sphere.glb");

				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Cone"))
			{
				auto e = m_CurrentScene->CreateEntity("Cone");
				e.AddComponent<MeshComponent>().LoadMeshSource(m_RHI, "assets/models/basic/cone.glb");

				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Cylinder"))
			{
				auto e = m_CurrentScene->CreateEntity("Cylinder");
				e.AddComponent<MeshComponent>().LoadMeshSource(m_RHI, "assets/models/basic/cylinder.glb");

				ImGui::CloseCurrentPopup();
			}

			ImGui::EndMenu();
		}
	}

	void SceneHierarchy::SetCurrentScene(Scene* current_scene)
	{
		m_CurrentScene = current_scene;
	}

	void SceneHierarchy::EntityProperties()
	{
		ImGui::Begin("Entity Properties", &open_entity_properties, ImGuiWindowFlags_NoCollapse);

		Entity selected_entity = m_CurrentScene->GetSelectedEntity();
		if (!selected_entity) goto end;

		if (selected_entity.HasComponent<TagComponent>())
		{
			auto& tag = selected_entity.GetComponent<TagComponent>().tag;
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
				bool mesh_component = !selected_entity.HasComponent<MeshComponent>();
				bool light_component = !selected_entity.HasComponent<PointLightComponent>() && !selected_entity.HasComponent<DirectionalLightComponent>();

				if (ImGui::MenuItem("Mesh Component", 0, false, mesh_component))
				{
					selected_entity.AddComponent<MeshComponent>();
					ImGui::CloseCurrentPopup();
				}

				if (ImGui::MenuItem("Point Light Component", 0, false, light_component))
				{
					selected_entity.AddComponent<PointLightComponent>();
					ImGui::CloseCurrentPopup();
				}

				if (ImGui::MenuItem("Directional Light Component", 0, false, light_component))
				{
					selected_entity.AddComponent<DirectionalLightComponent>();
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}
		}

		DrawComponentProperty<TransformComponent>(selected_entity, "Transform", [&]() {
			auto& transform = selected_entity.GetComponent<TransformComponent>();
			UI::DrawVec3("Translation", transform.translation);
			glm::vec3 rotation = glm::degrees(transform.rotation);
			UI::DrawVec3("Rotation", rotation);
			transform.rotation = glm::radians(rotation);
			UI::DrawVec3("Scale", transform.scale, 1.0f);
		});

		ImGui::Spacing();
		ImGui::Spacing();

		DrawComponentProperty<MeshComponent>(selected_entity, "Mesh", [&]() {
			auto& mc = selected_entity.GetComponent<MeshComponent>();

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
				std::string new_path = Application::Get()->OpenFileDialog("Model formats (.gltf, .glb)\0*.gltf;*.glb\0");
				if (!new_path.empty())
				{
					mc.mesh_path = new_path;
					m_CurrentScene->AddPreparation([&]() {
						mc.LoadMeshSource(m_RHI);
					});
				}
			}
		});

		ImGui::Spacing();
		ImGui::Spacing();

		DrawComponentProperty<PointLightComponent>(selected_entity, "Point Light", [&]() {
			auto& pl = selected_entity.GetComponent<PointLightComponent>();
			auto& transform = selected_entity.GetComponent<TransformComponent>();

			pl.position = glm::vec4(transform.translation, 1.0f);

			UI::DrawColor("Color", pl.color);
			UI::DrawProperty("Constant", pl.constant_value, 0.05f, 0.0f, 1.0f);
			UI::DrawProperty("Linear", pl.linear_value, 0.05f, 0.0f, 1.0f);
			UI::DrawProperty("Quadratic", pl.quadratic_value, 0.05f, 0.0f, 1.0f);
		});

		ImGui::Spacing();
		ImGui::Spacing();

		DrawComponentProperty<DirectionalLightComponent>(selected_entity, "Directional Light", [&]() {
			auto& dl = selected_entity.GetComponent<DirectionalLightComponent>();
			auto& transform = selected_entity.GetComponent<TransformComponent>();

			dl.direction = glm::vec4(transform.rotation, 1.0f);

			UI::DrawProperty("Intensity", dl.intensity, 0.05f, 0.0f, 1.0f);
		});

	end: // goto end
		ImGui::End();
	}

	SceneHierarchy::SceneHierarchy(std::shared_ptr<IRHI>& rhi, Scene* current_scene)
	{
		m_RHI = rhi;
		m_CurrentScene = current_scene;
	}

	SceneHierarchy::~SceneHierarchy()
	{
	}

	void SceneHierarchy::Render()
	{
		if (open_entity_properties)
			EntityProperties();
		if (open_scene_hierarchy)
			DrawHierarchy();
	}

}