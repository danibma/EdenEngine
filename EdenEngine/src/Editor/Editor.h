#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui/ImguiHelper.h>

#include "Core/Memory.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/SceneHierarchy.h"
#include "Graphics/Renderer.h"

namespace Eden
{
	class Window;
	class EdenEd
	{
		bool m_bOpenStatisticsWindow = true;
		bool m_bOpenSceneProperties = true;
		bool m_bOpenPipelinesPanel = true;
		bool m_bOpenMemoryPanel = true;
		int m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
		glm::vec2 m_ViewportPos;
		bool m_bIsViewportFocused = false;
		bool m_bIsViewportHovered = false;
		std::unique_ptr<ContentBrowserPanel> m_ContentBrowserPanel;
		std::unique_ptr<SceneHierarchy> m_SceneHierarchy;
		RenderPass m_ImGuiPass;

	private:
		void UI_Dockspace();
		void UI_Viewport();
		void UI_StatisticsWindow();
		void UI_DrawGizmo();
		void UI_PipelinesPanel();
		void UI_SceneProperties();
		void UI_MemoryPanel();
		void EditorInput();
		std::pair<uint32_t, uint32_t> GetViewportMousePos();

	public:
		void Init(Window* window);
		void Update();
		void Shutdown();
	};
}
