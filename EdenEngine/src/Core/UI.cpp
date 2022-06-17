#include "UI.h"

namespace Eden
{
	static float UI::AlignToFloat(Align alignment)
	{
		switch (alignment)
		{
		case Align::Left:
			return 0.0f;
			break;
		case Align::Center:
			return 0.5f;
			break;
		case Align::Right:
			return 1.0f;
			break;
		}
	}

	bool UI::Button(const char* label, Align alignment)
	{
		float align = AlignToFloat(alignment);
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(label).x) * align); // center text
		return ImGui::Button(label);
	}

	void UI::Text(const char* text, Align alignment)
	{
		float align = AlignToFloat(alignment);
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(text).x) * align); // center text
		ImGui::Text(text);
	}

	void UI::TextDisabled(const char* text, Align alignment)
	{
		float align = AlignToFloat(alignment);
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(text).x) * align); // center text
		ImGui::TextDisabled(text);
	}

	void UI::CenterWindow()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImVec2 window_size = ImGui::GetWindowSize();
		ImGui::SetWindowPos(ImVec2((viewport->Size.x / 2) - (window_size.x / 2), (viewport->Size.y / 2) - (window_size.y / 2)));
	}

}
