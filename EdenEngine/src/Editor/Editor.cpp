#include "Editor.h"

#include "Icons/IconsFontAwesome6.h"
#include "Core/Window.h"
#include "Core/Application.h"
#include "Core/Assertions.h"
#include "Core/Input.h"
#include "Math/Math.h"
#include "Utilities/Utils.h"
#include "Scene/SceneSerializer.h"
#include "Scene/Components.h"
#include "Graphics/Renderer.h"
#include "Scene/Entity.h"
#include "stdio.h"
#include "Graphics/RHI.h"

namespace Eden
{
	static std::unordered_map<const char*, Texture> g_EditorIcons;
	bool EdenEd::s_bIsTitleBarHovered = false;

	void EdenEd::UI_Dockspace()
	{
		ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;
		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
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
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 8.0f));
		if (ImGui::BeginMainMenuBar())
		{
			ImGui::PopStyleVar();
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New Scene", "Ctrl+N"))
					Renderer::NewScene();

				if (ImGui::MenuItem("Open Scene", "Ctrl+O"))
					Renderer::OpenSceneDialog();

				if (ImGui::MenuItem("Save Scene", "Ctrl-S"))
					Renderer::SaveScene();

				if (ImGui::MenuItem("Save Scene As", "Ctrl+Shift+S"))
					Renderer::SaveSceneAs();

				if (ImGui::MenuItem("Exit"))
					Application::Get()->RequestClose();

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View"))
			{
				ImGui::MenuItem("Statistics", NULL, &m_bOpenStatisticsWindow);
				ImGui::MenuItem("Memory Inspector", NULL, &m_bOpenMemoryPanel);
				ImGui::MenuItem("Inspector", NULL, &m_SceneHierarchy->bOpenInspector);
				ImGui::MenuItem("Hierarchy", NULL, &m_SceneHierarchy->bOpenHierarchy);
				ImGui::MenuItem("Scene Properties", NULL, &m_bOpenSceneProperties);
				ImGui::MenuItem("Pipelines Panel", NULL, &m_bOpenPipelinesPanel);
				ImGui::MenuItem("Content Browser", NULL, &m_ContentBrowserPanel->bOpenContentBrowser);

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("World"))
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

			const std::string title = Application::Get()->GetWindowTitle();
			ImGui::SetCursorPosX(ImGui::GetWindowSize().x * 0.5f - (ImGui::CalcTextSize(title.c_str()).x / 2.0f));
			ImGui::SetCursorPosY(5.0f);
			ImGui::Image((ImTextureID)Renderer::GetTextureID(&g_EditorIcons["EdenIcon"]), ImVec2(22, 22));
			ImGui::Text(title.c_str());

			const float buttonSize = 12.0f;

			char fpsText[256];
			snprintf(fpsText, 256, "FPS: %.0f | CPU: %.1fms | GPU: %.1fms | API: %s", 
				(1000.0f / Application::Get()->GetDeltaTime()) / 1000.0f, 
				Application::Get()->GetDeltaTime() * 1000.0f, 
				Renderer::GetRenderTimer().elapsedTime, 
				Utils::APIToString(Renderer::GetCurrentAPI()));
			ImGui::SetCursorPosX((ImGui::GetWindowSize().x - 400.0f));
			ImGui::Text(fpsText);
			ImTextureID closeID = reinterpret_cast<ImTextureID>(Renderer::GetTextureID(&g_EditorIcons["Close"]));
			ImTextureID minimizeID = reinterpret_cast<ImTextureID>(Renderer::GetTextureID(&g_EditorIcons["Minimize"]));
			ImTextureID maximizeID = reinterpret_cast<ImTextureID>(Renderer::GetTextureID(&g_EditorIcons["Maximize"]));
			ImTextureID restoreID = reinterpret_cast<ImTextureID>(Renderer::GetTextureID(&g_EditorIcons["Restore"]));
			if (!Application::Get()->IsWindowMaximized())
				restoreID = maximizeID;

			const float buttonTopPadding = 6.0f;
			ImGui::SetCursorPosY(buttonTopPadding);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			if (ImGui::ImageButton(minimizeID, ImVec2(buttonSize, buttonSize)))
			{
				Application::Get()->MinimizeWindow();
			}
			ImGui::SetCursorPosY(buttonTopPadding);
			if (ImGui::ImageButton(restoreID, ImVec2(buttonSize, buttonSize)))
			{
				Application::Get()->MaximizeWindow();
			}
			ImGui::SetCursorPosY(buttonTopPadding);
			if (ImGui::ImageButton(closeID, ImVec2(buttonSize, buttonSize)))
			{
				Application::Get()->RequestClose();
			}
			ImGui::PopStyleColor();

			s_bIsTitleBarHovered = !ImGui::IsAnyItemHovered() && ImGui::IsWindowHovered();
			ImGui::EndMainMenuBar();
		}

		ImGui::End();
	}

	void EdenEd::UI_SceneProperties()
	{
		ImGui::Begin(ICON_FA_TV " Scene Properties##sceneProperties", &m_bOpenSceneProperties, ImGuiWindowFlags_NoCollapse);
		ImGui::Text("Compute Shader test:");
		ImVec2 imageSize;
		imageSize.x = static_cast<float>(Renderer::GetComputeTestOutputImage().desc.width);
		imageSize.y = static_cast<float>(Renderer::GetComputeTestOutputImage().desc.height);
		Renderer::EnsureMsgResourceState(&Renderer::GetComputeTestOutputImage(), ResourceState::kPixelShader);
		ImGui::Image((ImTextureID)Renderer::GetTextureID(&Renderer::GetComputeTestOutputImage()), imageSize);
		ImGui::Separator();

		ImGui::Checkbox("Enable Skybox", &Renderer::IsSkyboxEnabled());
		ImGui::Separator();
		UI::DrawProperty("Exposure", Renderer::GetSceneSettings().exposure, 0.1f, 0.1f, 5.0f);
		ImGui::End();
	}

	void EdenEd::UI_StatisticsWindow()
	{
		ImGui::Begin(ICON_FA_CHART_PIE " Statistcs##statistics", &m_bOpenStatisticsWindow, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Text("CPU frame time: %.3fms(%.1fFPS)", Application::Get()->GetDeltaTime() * 1000.0f, (1000.0f / Application::Get()->GetDeltaTime()) / 1000.0f);
		ImGui::Text("GPU frame time: %.3fms", Renderer::GetRenderTimer().elapsedTime);
		ImGui::Text("Compute test time: %.3fms", Renderer::GetComputeTimer().elapsedTime);
		ImGui::End();
	}

	void EdenEd::UI_DrawGizmo()
	{
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();

		float windowX = m_ViewportPos.x;
		float windowY = m_ViewportPos.y;
		ImGuizmo::SetRect(windowX, windowY, Renderer::GetViewportSize().x, Renderer::GetViewportSize().y);

		auto& transform_component = Renderer::GetCurrentScene()->GetSelectedEntity().GetComponent<TransformComponent>();
		glm::mat4 transform = transform_component.GetTransform();

		ImGuizmo::Manipulate(glm::value_ptr(Renderer::GetViewMatrix()), glm::value_ptr(Renderer::GetProjectionMatrix()), static_cast<ImGuizmo::OPERATION>(m_GizmoType), ImGuizmo::LOCAL, glm::value_ptr(transform));

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

	void EdenEd::UI_PipelinesPanel()
	{
		ImGui::Begin(ICON_FA_PUZZLE_PIECE " Pipelines##pipelines");
		ImGui::Columns(2);
		ImGui::NextColumn();
		if (ImGui::Button("Reload all pipelines"))
		{
			for (auto& pipeline : Renderer::GetPipelines())
				Renderer::GetCurrentScene()->AddPreparation([&]() { Renderer::ReloadPipeline(&pipeline.second); });
		}
		ImGui::Columns(1);
		for (auto& pipeline : Renderer::GetPipelines())
		{
			ImGui::PushID(pipeline.second.debugName.c_str());
			ImGui::Columns(2);
			ImGui::Text(pipeline.first);
			ImGui::NextColumn();
			if (ImGui::Button("Reload pipeline"))
				Renderer::GetCurrentScene()->AddPreparation([&]() { Renderer::ReloadPipeline(&pipeline.second); });
			ImGui::Columns(1);
			ImGui::PopID();
		}
		ImGui::End();
	}

	void EdenEd::UI_Viewport()
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
		Renderer::SetViewportSize(viewportSize.x, viewportSize.y);
		auto viewportPos = ImGui::GetWindowPos();
		m_ViewportPos = { viewportPos.x, viewportPos.y };
		Renderer::SetCameraPosition(viewportPos.x, viewportPos.y);

		m_bIsViewportFocused = ImGui::IsWindowFocused();
		m_bIsViewportHovered = ImGui::IsWindowHovered();

		ImGui::Image((ImTextureID)Renderer::GetTextureID(&Renderer::GetFinalImage()), viewportSize);

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
					Renderer::OpenScene(path);
					break;
				case EdenExtension::kModel:
				{
					auto e = Renderer::GetCurrentScene()->CreateEntity(path.stem().string());
					auto& mc = e.AddComponent<MeshComponent>();
					mc.meshPath = path.string();
					Renderer::GetCurrentScene()->AddPreparation([&]() {
						mc.LoadMeshSource();
					});
				}
				break;
				case EdenExtension::kEnvironmentMap:
					Renderer::GetCurrentScene()->AddPreparation([&]() {
						Renderer::SetNewSkybox(path.string().c_str());
					});
					break;
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (Input::GetKeyDown(ED_KEY_DELETE) && ImGui::IsWindowFocused())
		{
			Renderer::GetCurrentScene()->AddPreparation([&]() {
				auto selectedEntity = Renderer::GetCurrentScene()->GetSelectedEntity();
				Renderer::GetCurrentScene()->DeleteEntity(selectedEntity);
			});
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void EdenEd::UI_MemoryPanel()
	{
		ImGui::Begin(ICON_FA_BUG " Memory##memory", &m_bOpenMemoryPanel);
		ImGui::Text("Memory Sources:");
		{
			std::string total_allocated = "Total Allocated: " + Utils::BytesToString(Memory::MemoryManager::GetTotalAllocated());
			ImGui::Text(total_allocated.c_str());
		}
		{
			std::string total_freed = "Total Freed: " + Utils::BytesToString(Memory::MemoryManager::GetTotalFreed());
			ImGui::Text(total_freed.c_str());
		}
		{
			std::string currentUsage = "Current Usage: " + Utils::BytesToString(Memory::MemoryManager::GetCurrentAllocated());
			ImGui::Text(currentUsage.c_str());
		}
		ImGui::Separator();
		ImGui::Text("Memory Allocation sources:");
		for (const auto& source : Memory::MemoryManager::GetCurrentAllocatedSources())
		{
			std::string currentUsage = Utils::BytesToString(source.first);
			ImGui::Text("    %s: %s", source.second, currentUsage.c_str());
		}
		ImGui::End();
	}

	void EdenEd::UI_OutputLog()
	{
		ImGui::Begin(ICON_FA_CIRCLE_INFO " Output Log##outputlog", &m_bOpenOutputLog);
		std::vector<std::string> msgs = Log::GetOutputLog();
		size_t currentAmount = msgs.size();

		for (auto& logMsg : msgs)
		{
			if (logMsg.find("trace") != std::string::npos)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150, 150, 150, 255));
			}
			else if (logMsg.find("info") != std::string::npos)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 200, 0, 255));
			}
			else if (logMsg.find("warn") != std::string::npos)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 200, 200, 255));
			}
			else if (logMsg.find("error") != std::string::npos)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 0, 0, 200));
			}
			else if (logMsg.find("critical") != std::string::npos)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
			}

			ImGui::Text(logMsg.c_str());
			ImGui::PopStyleColor();
		}

		if (currentAmount > m_AmountOfLogMsgs)
			ImGui::SetScrollHereY(1.0f);
		m_AmountOfLogMsgs = currentAmount;
		ImGui::End();
	}

	std::pair<uint32_t, uint32_t> EdenEd::GetViewportMousePos()
	{
		std::pair<uint32_t, uint32_t> viewportMousePos;
		auto pos = Input::GetMousePos();
		viewportMousePos.first = static_cast<uint32_t>(pos.first - m_ViewportPos.x);
		viewportMousePos.second = static_cast<uint32_t>(pos.second - m_ViewportPos.y);

		return viewportMousePos;
	}

	void EdenEd::EditorInput()
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
				if (mousePos.first >= 1 && mousePos.first <= Renderer::GetViewportSize().x && mousePos.second >= 1 && mousePos.second <= Renderer::GetViewportSize().y)
					Renderer::GetPixel(mousePos.first, mousePos.second, pixel_color);
				if (pixel_color.x >= 0)
				{
					entt::entity entityId = static_cast<entt::entity>(std::round(pixel_color.x));
					Entity entity = { entityId, Renderer::GetCurrentScene() };
					Renderer::GetCurrentScene()->SetSelectedEntity(entity);
				}
				else
				{
					entt::entity entityId = static_cast<entt::entity>(-1);
					Entity entity = { entityId, Renderer::GetCurrentScene() };
					Renderer::GetCurrentScene()->SetSelectedEntity(entity);
				}
			}
		}

		if (Input::GetKey(ED_KEY_SHIFT) && Input::GetKey(ED_KEY_CONTROL) && Input::GetKeyDown(ED_KEY_S))
			Renderer::SaveSceneAs();
		if (Input::GetKey(ED_KEY_CONTROL) && Input::GetKeyDown(ED_KEY_O))
			Renderer::OpenSceneDialog();
		if (Input::GetKey(ED_KEY_CONTROL) && Input::GetKeyDown(ED_KEY_N))
			Renderer::NewScene();
		if (Input::GetKey(ED_KEY_CONTROL) && Input::GetKeyDown(ED_KEY_S))
			Renderer::SaveScene();
	}

	Texture EdenEd::GetEditorIcon(const char* iconName)
	{
		return g_EditorIcons[iconName];
	}

	void EdenEd::Init(Window* window)
	{
		Renderer::SetViewportSize(static_cast<float>(window->GetWidth()), static_cast<float>(window->GetHeight()));
		m_ViewportPos = { 0, 0 };

		{
			RenderPassDesc desc = {};
			desc.debugName = "ImGuiPass";
			desc.attachmentsFormats = { Format::kRGBA32_FLOAT, Format::Depth };
			desc.bIsSwapchainTarget = true;
			desc.bIsImguiPass = true;
			desc.width = static_cast<uint32_t>(Renderer::GetViewportSize().x);
			desc.height = static_cast<uint32_t>(Renderer::GetViewportSize().y);
			Renderer::CreateRenderpass(&m_ImGuiPass, &desc);
			Renderer::SetSwapchainTarget(&m_ImGuiPass);
			Renderer::EnableImGui();
		}

		Renderer::CreateTexture(&g_EditorIcons["File"], "assets/editor/file.png", false);
		Renderer::CreateTexture(&g_EditorIcons["Folder"], "assets/editor/folder.png", false);
		Renderer::CreateTexture(&g_EditorIcons["Back"], "assets/editor/icon_back.png", false);
		Renderer::CreateTexture(&g_EditorIcons["Close"], "assets/editor/window_close.png", false);
		Renderer::CreateTexture(&g_EditorIcons["Minimize"], "assets/editor/window_minimize.png", false);
		Renderer::CreateTexture(&g_EditorIcons["Maximize"], "assets/editor/window_maximize.png", false);
		Renderer::CreateTexture(&g_EditorIcons["Restore"], "assets/editor/window_restore.png", false);
		Renderer::CreateTexture(&g_EditorIcons["EdenIcon"], "assets/editor/icon.png", false);

		m_ContentBrowserPanel = std::make_unique<ContentBrowserPanel>();
		m_SceneHierarchy = std::make_unique<SceneHierarchy>();
	}

	void EdenEd::Update()
	{
		EditorInput();

		Renderer::BeginRenderPass(&m_ImGuiPass);
		//ImGui::ShowDemoWindow(nullptr);

		UI_Dockspace();
		UI_Viewport();
		if (m_bOpenOutputLog)
			UI_OutputLog();
		m_ContentBrowserPanel->Render();
		m_SceneHierarchy->Render();
		if (m_bOpenStatisticsWindow)
			UI_StatisticsWindow();
		if (Renderer::GetCurrentScene()->GetSelectedEntity())
			UI_DrawGizmo();
		if (m_bOpenPipelinesPanel)
			UI_PipelinesPanel();
		if (m_bOpenSceneProperties)
			UI_SceneProperties();
		if (m_bOpenMemoryPanel)
			UI_MemoryPanel();

		Renderer::EndRenderPass(&m_ImGuiPass);
	}

	void EdenEd::Shutdown()
	{
		g_EditorIcons.clear();
	}
}