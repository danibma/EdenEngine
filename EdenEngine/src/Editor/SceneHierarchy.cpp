#include "SceneHierarchy.h"

#include <imgui/imgui.h>

#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "UI/UI.h"
#include "Core/Input.h"
#include "Core/Application.h"
#include "Graphics/RHI.h"

namespace Eden
{
	SceneHierarchy::SceneHierarchy(std::shared_ptr<IRHI>& rhi, Scene* currentScene)
	{
		m_RHI = rhi;
		m_CurrentScene = currentScene;
	}

	SceneHierarchy::~SceneHierarchy()
	{
	}

	void SceneHierarchy::DrawHierarchy()
	{
		ImGui::Begin(ICON_FA_BARS_STAGGERED  " Scene Hierarchy##hierarchy", &bOpenHierarchy, ImGuiWindowFlags_NoCollapse);
		
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
				
				if (ImGui::MenuItem("Duplicate", "Ctrl-D"))
					DuplicateSelectedEntity();
				if (ImGui::MenuItem("Delete", "Del"))
					DeleteSelectedEntity();

				ImGui::Separator();
				EntityMenu();

				ImGui::EndPopup();
			}

			ImGui::PopID();
		}

		// Scene hierarchy popup to create new entities
		if (Input::GetMouseButton(ED_MOUSE_RIGHT) && ImGui::IsWindowHovered())
			ImGui::OpenPopup("hierarchyPopup");

		if (ImGui::BeginPopup("hierarchyPopup"))
		{
			EntityMenu();
			ImGui::EndPopup();
		}

		if (Input::GetKeyDown(ED_KEY_DELETE) && ImGui::IsWindowFocused())
			DeleteSelectedEntity();
		if (Input::GetKey(ED_KEY_CONTROL) && Input::GetKeyDown(ED_KEY_D) && ImGui::IsWindowFocused())
			DuplicateSelectedEntity();

		ImGui::End();
	}

	template <typename T>
	static void DrawComponentProperty(Entity selectedEntity, const char* title, std::function<void()> func)
	{
		if (selectedEntity.HasComponent<T>())
		{
			ImGui::PushID((void*)typeid(T).hash_code());

			bool bOpen = ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap);
			ImGui::SameLine(ImGui::GetWindowWidth() - 25.0f);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			if (ImGui::Button("...", ImVec2(20, 19)))
			{
				ImGui::OpenPopup("componentSettings");
			}
			ImGui::PopStyleColor();

			if (ImGui::BeginPopup("componentSettings"))
			{
				bool bRemoveComponent = true;
				if (std::is_same<T, TransformComponent>::value)
					bRemoveComponent = false;

				if (ImGui::MenuItem("Remove Component", nullptr, false, bRemoveComponent))
				{
					selectedEntity.RemoveComponent<T>();
					bOpen = false;

					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}
			ImGui::PopID();

			if (bOpen)
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
				m_CurrentScene->AddPreparation([&]()
				{
					auto e = m_CurrentScene->CreateEntity("Cube");
					e.AddComponent<MeshComponent>().LoadMeshSource(m_RHI, "assets/models/basic/cube.glb");
				});

				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Plane"))
			{
				m_CurrentScene->AddPreparation([&]()
				{
					auto e = m_CurrentScene->CreateEntity("Plane");
					e.AddComponent<MeshComponent>().LoadMeshSource(m_RHI, "assets/models/basic/plane.glb");
				});

				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Sphere"))
			{
				m_CurrentScene->AddPreparation([&]()
				{
					auto e = m_CurrentScene->CreateEntity("Sphere");
					e.AddComponent<MeshComponent>().LoadMeshSource(m_RHI, "assets/models/basic/sphere.glb");
				});

				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Cone"))
			{
				m_CurrentScene->AddPreparation([&]()
				{
					auto e = m_CurrentScene->CreateEntity("Cone");
					e.AddComponent<MeshComponent>().LoadMeshSource(m_RHI, "assets/models/basic/cone.glb");
				});

				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("Cylinder"))
			{
				m_CurrentScene->AddPreparation([&]()
				{
					auto e = m_CurrentScene->CreateEntity("Cylinder");
					e.AddComponent<MeshComponent>().LoadMeshSource(m_RHI, "assets/models/basic/cylinder.glb");
				});

				ImGui::CloseCurrentPopup();
			}

			ImGui::EndMenu();
		}
	}

	void SceneHierarchy::SetCurrentScene(Scene* currentScene)
	{
		m_CurrentScene = currentScene;
	}

	void SceneHierarchy::EntityProperties()
	{
		ImGui::Begin(ICON_FA_CIRCLE_INFO " Inspector##inspector", &bOpenInspector, ImGuiWindowFlags_NoCollapse);

		Entity selectedEntity = m_CurrentScene->GetSelectedEntity();
		if (!selectedEntity) goto end;

		if (selectedEntity.HasComponent<TagComponent>())
		{
			auto& tag = selectedEntity.GetComponent<TagComponent>().tag;
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
				ImGui::OpenPopup("addcPopup");

			if (ImGui::BeginPopup("addcPopup"))
			{
				bool bHasMeshComponent = !selectedEntity.HasComponent<MeshComponent>();
				bool bHasLightComponent = !selectedEntity.HasComponent<PointLightComponent>() && !selectedEntity.HasComponent<DirectionalLightComponent>();

				if (ImGui::MenuItem("Mesh Component", 0, false, bHasMeshComponent))
				{
					selectedEntity.AddComponent<MeshComponent>();
					ImGui::CloseCurrentPopup();
				}

				if (ImGui::MenuItem("Point Light Component", 0, false, bHasLightComponent))
				{
					selectedEntity.AddComponent<PointLightComponent>();
					ImGui::CloseCurrentPopup();
				}

				if (ImGui::MenuItem("Directional Light Component", 0, false, bHasLightComponent))
				{
					selectedEntity.AddComponent<DirectionalLightComponent>();
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}
		}

		DrawComponentProperty<TransformComponent>(selectedEntity, "Transform", [&]() {
			auto& transform = selectedEntity.GetComponent<TransformComponent>();
			UI::DrawVec3("Translation", transform.translation);
			glm::vec3 rotation = glm::degrees(transform.rotation);
			UI::DrawVec3("Rotation", rotation);
			transform.rotation = glm::radians(rotation);
			UI::DrawVec3("Scale", transform.scale, 1.0f);
		});

		ImGui::Spacing();
		ImGui::Spacing();

		DrawComponentProperty<MeshComponent>(selectedEntity, "Mesh", [&]() {
			auto& mc = selectedEntity.GetComponent<MeshComponent>();

			ImGui::Text("File Path");
			ImGui::SameLine();
			constexpr size_t num_chars = 256;
			char buffer[num_chars];
			memset(buffer, 0, num_chars);
			memcpy(buffer, mc.meshPath.c_str(), mc.meshPath.length());
			ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.9f);
			ImGui::InputText("###meshpath", buffer, num_chars, ImGuiInputTextFlags_ReadOnly);
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (UI::AlignedButton("...", UI::Align::Right))
			{
				std::string newPath = Application::Get()->OpenFileDialog("Model formats (.gltf, .glb)\0*.gltf;*.glb\0");
				if (!newPath.empty())
				{
					mc.meshPath = newPath;
					m_CurrentScene->AddPreparation([&]() {
						mc.LoadMeshSource(m_RHI);
					});
				}
			}
		});

		ImGui::Spacing();
		ImGui::Spacing();

		DrawComponentProperty<PointLightComponent>(selectedEntity, "Point Light", [&]() {
			auto& pl = selectedEntity.GetComponent<PointLightComponent>();
			auto& transform = selectedEntity.GetComponent<TransformComponent>();

			pl.position = glm::vec4(transform.translation, 1.0f);

			UI::DrawColor("Color", pl.color);
			UI::DrawProperty("Intensity", pl.intensity, 0.5f, 0.01f, 0.0f);
		});

		ImGui::Spacing();
		ImGui::Spacing();

		DrawComponentProperty<DirectionalLightComponent>(selectedEntity, "Directional Light", [&]() {
			auto& dl = selectedEntity.GetComponent<DirectionalLightComponent>();
			auto& transform = selectedEntity.GetComponent<TransformComponent>();

			dl.direction = glm::vec4(transform.rotation, 1.0f);

			UI::DrawProperty("Intensity", dl.intensity, 0.1f, 0.01f, 0.0f);
		});

	end: // goto end
		ImGui::End();
	}

	void SceneHierarchy::DuplicateSelectedEntity()
	{
		m_CurrentScene->AddPreparation([&]() {
			auto newEntity = m_CurrentScene->DuplicateEntity(m_CurrentScene->GetSelectedEntity());
			if (newEntity.HasComponent<MeshComponent>())
			{
				auto& mc = newEntity.GetComponent<MeshComponent>();
				mc.LoadMeshSource(m_RHI);
			}

			m_CurrentScene->SetSelectedEntity(newEntity);
		});
	}

	void SceneHierarchy::DeleteSelectedEntity()
	{
		m_CurrentScene->AddPreparation([&]() {
			m_CurrentScene->DeleteEntity(m_CurrentScene->GetSelectedEntity());
		});
	}

	void SceneHierarchy::Render()
	{
		if (bOpenInspector)
			EntityProperties();
		if (bOpenHierarchy)
			DrawHierarchy();
	}

}