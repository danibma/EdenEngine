#include "UI.h"
#include "glm\gtc\type_ptr.hpp"

namespace Eden::UI
{
	static float AlignToFloat(Align alignment)
	{
		switch (alignment)
		{
		case Align::Left:
			return 0.05f;
			break;
		case Align::Center:
			return 0.5f;
			break;
		case Align::Right:
			return 0.95f;
			break;
		}
	}

	bool AlignedButton(const char* label, Align alignment)
	{
		float align = AlignToFloat(alignment);
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(label).x) * align); // center text
		return ImGui::Button(label);
	}

	void AlignedText(const char* text, Align alignment)
	{
		float align = AlignToFloat(alignment);
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(text).x) * align); // center text
		ImGui::Text(text);
	}

	void AlignedTextDisabled(const char* text, Align alignment)
	{
		float align = AlignToFloat(alignment);
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(text).x) * align); // center text
		ImGui::TextDisabled(text);
	}

	void CenterWindow()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImVec2 window_size = ImGui::GetWindowSize();
		ImGui::SetWindowPos(ImVec2((viewport->Size.x / 2) - (window_size.x / 2), (viewport->Size.y / 2) - (window_size.y / 2)));
	}

	static void Styles()
	{
		auto& style = ImGui::GetStyle();

		style.WindowPadding = ImVec2(6, 4);
		style.WindowRounding = 0.0f;
		style.FramePadding = ImVec2(5, 2);
		style.TabRounding = 1.0f;
		style.FrameRounding = 1.0f;
		style.ItemSpacing = ImVec2(7, 5);
		style.ItemInnerSpacing = ImVec2(3, 1);
		style.TouchExtraPadding = ImVec2(0, 0);
		style.IndentSpacing = 6.0f;
		style.ScrollbarSize = 12.0f;
		style.ScrollbarRounding = 16.0f;
		style.GrabMinSize = 20.0f;
		style.GrabRounding = 2.0f;
		style.WindowMinSize = ImVec2(1, 1);    // Minimum window size

		style.WindowTitleAlign.x = 0.0f;

		style.Colors[ImGuiCol_Border] = ImVec4(0.539f, 0.479f, 0.255f, 0.162f);
		style.FrameBorderSize = 0.0f;
		style.WindowBorderSize = 1.0f;
	}

	// Based on https://github.com/ocornut/imgui/issues/707#issuecomment-430613104
	void CherryTheme()
	{
		auto& style = ImGui::GetStyle();

		//========================================================
		/// Colours
	#define HI(v)   ImVec4(0.502f, 0.075f, 0.256f, v)
	#define MED(v)  ImVec4(0.455f, 0.198f, 0.301f, v)
	#define LOW(v)  ImVec4(0.232f, 0.201f, 0.271f, v)
	#define BG(v)   ImVec4(0.200f, 0.220f, 0.270f, v)
	#define TEXT_COLOR(v) ImVec4(1.0f, 1.0f, 1.0f, v)

		style.Colors[ImGuiCol_Text] = TEXT_COLOR(0.78f);
		style.Colors[ImGuiCol_TextDisabled] = TEXT_COLOR(0.28f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.17f, 0.70f);
		style.Colors[ImGuiCol_ChildBg] = BG(0.58f);
		style.Colors[ImGuiCol_PopupBg] = BG(0.9f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.31f, 0.31f, 1.00f, 0.00f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_FrameBg] = BG(1.00f);
		style.Colors[ImGuiCol_FrameBgHovered] = MED(0.78f);
		style.Colors[ImGuiCol_FrameBgActive] = MED(1.00f);
		style.Colors[ImGuiCol_TitleBg] = LOW(1.00f);
		style.Colors[ImGuiCol_TitleBgActive] = HI(1.00f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = BG(0.75f);
		style.Colors[ImGuiCol_DockingPreview] = HI(1.0f);
		style.Colors[ImGuiCol_Tab] = LOW(1.0f);
		style.Colors[ImGuiCol_TabActive] = HI(1.0f);
		style.Colors[ImGuiCol_TabUnfocusedActive] = LOW(1.0f);
		style.Colors[ImGuiCol_TabHovered] = MED(1.0f);
		style.Colors[ImGuiCol_MenuBarBg] = BG(1.0f);
		style.Colors[ImGuiCol_ScrollbarBg] = BG(1.00f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.09f, 0.15f, 0.16f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = MED(0.78f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = MED(1.00f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.71f, 0.22f, 0.27f, 1.00f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.71f, 0.22f, 0.27f, 1.00f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
		style.Colors[ImGuiCol_ButtonHovered] = MED(0.86f);
		style.Colors[ImGuiCol_ButtonActive] = MED(1.00f);
		style.Colors[ImGuiCol_Header] = MED(0.76f);
		style.Colors[ImGuiCol_HeaderHovered] = MED(0.86f);
		style.Colors[ImGuiCol_HeaderActive] = HI(1.00f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.47f, 0.77f, 0.83f, 0.04f);
		style.Colors[ImGuiCol_ResizeGripHovered] = MED(0.78f);
		style.Colors[ImGuiCol_ResizeGripActive] = MED(1.00f);
		style.Colors[ImGuiCol_PlotLines] = TEXT_COLOR(0.63f);
		style.Colors[ImGuiCol_PlotLinesHovered] = MED(1.00f);
		style.Colors[ImGuiCol_PlotHistogram] = TEXT_COLOR(0.63f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = MED(1.00f);
		style.Colors[ImGuiCol_TextSelectedBg] = MED(0.43f);
		style.Colors[ImGuiCol_ModalWindowDimBg] = BG(0.73f);

		//========================================================
		Styles();
	}

	// Based on Hazel Engine Theme
	void HazelTheme()
	{
		auto& style = ImGui::GetStyle();
		auto& colors = ImGui::GetStyle().Colors;

		//========================================================
		/// Colours
		constexpr auto accent = IM_COL32(236, 158, 36, 255);
		constexpr auto highlight = IM_COL32(39, 185, 242, 255);
		constexpr auto niceBlue = IM_COL32(83, 232, 254, 255);
		constexpr auto compliment = IM_COL32(78, 151, 166, 255);
		constexpr auto background = IM_COL32(36, 36, 36, 255);
		constexpr auto backgroundDark = IM_COL32(26, 26, 26, 255);
		constexpr auto titlebar = IM_COL32(21, 21, 21, 200);
		constexpr auto propertyField = IM_COL32(15, 15, 15, 255);
		constexpr auto text = IM_COL32(192, 192, 192, 255);
		constexpr auto textBrighter = IM_COL32(210, 210, 210, 255);
		constexpr auto textDarker = IM_COL32(128, 128, 128, 255);
		constexpr auto muted = IM_COL32(77, 77, 77, 255);
		constexpr auto groupHeader = IM_COL32(47, 47, 47, 255);
		//constexpr auto selection				= IM_COL32(191, 177, 155, 255);
		//constexpr auto selectionMuted			= IM_COL32(59, 57, 45, 255);
		constexpr auto selection = IM_COL32(237, 192, 119, 255);
		constexpr auto selectionMuted = IM_COL32(237, 201, 142, 23);

		//constexpr auto backgroundPopup			= IM_COL32(63, 73, 77, 255); // in between
		//constexpr auto backgroundPopup			= IM_COL32(63, 77, 76, 255); // most green
		constexpr auto backgroundPopup = IM_COL32(63, 70, 77, 255); // most blue

		// Headers
		colors[ImGuiCol_Header] = ImGui::ColorConvertU32ToFloat4(groupHeader);
		colors[ImGuiCol_HeaderHovered] = ImGui::ColorConvertU32ToFloat4(groupHeader);
		colors[ImGuiCol_HeaderActive] = ImGui::ColorConvertU32ToFloat4(groupHeader);

		// Buttons
		colors[ImGuiCol_Button] = ImColor(56, 56, 56, 200);
		colors[ImGuiCol_ButtonHovered] = ImColor(70, 70, 70, 255);
		colors[ImGuiCol_ButtonActive] = ImColor(56, 56, 56, 150);

		// Frame BG
		colors[ImGuiCol_FrameBg] = ImGui::ColorConvertU32ToFloat4(propertyField);
		colors[ImGuiCol_FrameBgHovered] = ImColor(255, 225, 135, 30);
		colors[ImGuiCol_FrameBgActive] = ImColor(255, 225, 135, 60);

		// Tabs
		colors[ImGuiCol_Tab] = ImGui::ColorConvertU32ToFloat4(titlebar);
		colors[ImGuiCol_TabHovered] = ImColor(255, 225, 135, 30);
		colors[ImGuiCol_TabActive] = ImColor(255, 225, 135, 60);
		colors[ImGuiCol_TabUnfocused] = ImGui::ColorConvertU32ToFloat4(titlebar);
		colors[ImGuiCol_TabUnfocusedActive] = colors[ImGuiCol_TabHovered];

		// Title
		colors[ImGuiCol_TitleBg] = ImGui::ColorConvertU32ToFloat4(titlebar);
		colors[ImGuiCol_TitleBgActive] = ImGui::ColorConvertU32ToFloat4(titlebar);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Resize Grip
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.91f, 0.91f, 0.91f, 0.25f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.67f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.46f, 0.46f, 0.46f, 0.95f);

		// Scrollbar
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.0f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.0f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.0f);

		// Check Mark
		colors[ImGuiCol_CheckMark] = ImColor(200, 200, 200, 255);

		// Slider
		colors[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.51f, 0.51f, 0.7f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.66f, 0.66f, 0.66f, 1.0f);

		// Text
		colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(text);

		// Checkbox
		colors[ImGuiCol_CheckMark] = ImGui::ColorConvertU32ToFloat4(text);

		// Separator
		colors[ImGuiCol_Separator] = ImGui::ColorConvertU32ToFloat4(backgroundDark);
		colors[ImGuiCol_SeparatorActive] = ImGui::ColorConvertU32ToFloat4(highlight);
		colors[ImGuiCol_SeparatorHovered] = ImColor(39, 185, 242, 150);

		// Window Background
		colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(titlebar);
		colors[ImGuiCol_ChildBg] = ImGui::ColorConvertU32ToFloat4(background);
		colors[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(backgroundPopup);
		colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(backgroundDark);

		// Tables
		colors[ImGuiCol_TableHeaderBg] = ImGui::ColorConvertU32ToFloat4(groupHeader);
		colors[ImGuiCol_TableBorderLight] = ImGui::ColorConvertU32ToFloat4(backgroundDark);

		// Menubar
		colors[ImGuiCol_MenuBarBg] = ImColor(21, 21, 21, 255);

		// Dockspace
		colors[ImGuiCol_DockingEmptyBg] = ImColor(255, 225, 135, 60);
		colors[ImGuiCol_DockingPreview] = ImColor(255, 225, 135, 60);

		//========================================================
		Styles();
	}

	void DrawVec3(const std::string& label, glm::vec3& values, float reset_value /*= 0.0f*/, float column_width /*= 100.0f*/)
	{
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, column_width);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 5 });

		float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize))
			values.x = reset_value;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
			values.y = reset_value;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
			values.z = reset_value;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();
	}

	void DrawProperty(const std::string& label, float& value, float column_width /* = 100.0f*/)
	{
		ImGui::PushID(label.c_str());
		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, column_width);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		ImGui::DragFloat("###X", &value);
		ImGui::PopItemWidth();
		ImGui::Columns(1);
		ImGui::PopID();
	}

	void DrawColor(const std::string& label, glm::vec4& values, float column_width /* = 100.0f*/)
	{
		ImGui::PushID(label.c_str());
		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, column_width);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		ImGui::ColorEdit4("###Color", glm::value_ptr(values));
		ImGui::PopItemWidth();
		ImGui::Columns(1);
		ImGui::PopID();
	}

}
