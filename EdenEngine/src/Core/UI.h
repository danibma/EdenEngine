#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

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


	bool Button(const char* label, Align alignment);
	void Text(const char* text, Align alignment);
	void TextDisabled(const char* text, Align alignment);
	void CenterWindow();
	
	// Themes
	static void Styles();
	void CherryTheme();
	void HazelTheme();
}

