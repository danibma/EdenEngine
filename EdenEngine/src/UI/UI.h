#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <string>
#include <glm/glm.hpp>

#include "ImGuizmo.h"

namespace Eden::UI
{
	enum class Themes
	{
		Cherry = 0,
		Hazel
	};
	inline Themes g_SelectedTheme = Themes::Hazel;

	enum class Align
	{
		Left,
		Center,
		Right
	};
	static float AlignToFloat(Align alignment);


	bool AlignedButton(const char* label, Align alignment);
	void AlignedText(const char* text, Align alignment);
	void AlignedTextDisabled(const char* text, Align alignment);
	void CenterWindow();
	void DrawVec3(const std::string& label, glm::vec3& values, float reset_value = 0.0f, float column_width = 100.0f);
	void DrawColor(const std::string& label, glm::vec4& values, float column_width = 100.0f);
	void DrawProperty(const std::string& label, float& value, float speed = 1.0f, float min = 0.0f, float max = 0.0f, float column_width = 100.0f);
	
	// Themes
	static void Styles();
	void CherryTheme();
	void HazelTheme();
}

