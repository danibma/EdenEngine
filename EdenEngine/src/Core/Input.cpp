#include "Input.h"

#include <windowsx.h>

#include "Log.h"
#include "imgui_internal.h"

namespace Eden
{
	bool Input::s_KeyDown[VK_OEM_CLEAR] = {};
	bool Input::s_PreviousKeyDown[VK_OEM_CLEAR] = {};
	std::pair<int64_t, int64_t> Input::s_MousePos;
	std::pair<int64_t, int64_t> Input::s_RelativeMousePos;
	float Input::s_MouseScrollDelta;
	InputMode Input::s_InputMode;

	void Input::UpdateInput()
	{
		memcpy(s_PreviousKeyDown, s_KeyDown, sizeof(s_KeyDown));
	}

	void Input::HandleInput(HWND hwnd, uint64_t message, uint64_t code, uint64_t lParam)
	{
		switch (message)
		{
		case WM_KEYUP: case WM_SYSKEYUP:
			s_KeyDown[code] = false;
			break;
		case WM_KEYDOWN: case WM_SYSKEYDOWN:
			s_KeyDown[code] = true;
			break;
		case WM_LBUTTONDOWN:
			s_KeyDown[VK_LBUTTON] = true;
			break;
		case WM_LBUTTONUP:
			s_KeyDown[VK_LBUTTON] = false;
			break;
		case WM_RBUTTONDOWN:
			s_KeyDown[VK_RBUTTON] = true;
			break;
		case WM_RBUTTONUP:
			s_KeyDown[VK_RBUTTON] = false;
			break;
		case WM_MBUTTONDOWN:
			s_KeyDown[VK_MBUTTON] = true;
			break;
		case WM_MBUTTONUP:
			s_KeyDown[VK_MBUTTON] = false;
			break;
		case WM_INPUT:
			if (GetCursorMode() == CursorMode::Hidden)
			{
				RAWINPUT raw;
				UINT raw_size = sizeof(raw);

				const UINT result_data = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &raw_size, sizeof(RAWINPUTHEADER));
				if (result_data == -1)
					ED_LOG_FATAL("Failed to get RawInputData!");

				if (raw.header.dwType == RIM_TYPEMOUSE)
				{
					s_RelativeMousePos.first += raw.data.mouse.lLastX;
					s_RelativeMousePos.second += raw.data.mouse.lLastY;
				}
			}
			break;
		case WM_MOUSEMOVE:
			if (GetCursorMode() == CursorMode::Visible)
			{
				POINT mouse_pos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				::ClientToScreen(hwnd, &mouse_pos);
				s_MousePos = { mouse_pos.x, mouse_pos.y };
			}
			break;
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL:
			s_MouseScrollDelta = GET_WHEEL_DELTA_WPARAM(code);
			break;
		}
	}

	bool Input::GetKey(KeyCode keycode)
	{
		return ((uint32_t)keycode < ARRAYSIZE(s_KeyDown) ? s_KeyDown[(uint32_t)keycode] : false);
	}

	bool Input::GetKeyUp(KeyCode keycode)
	{
		return ((uint32_t)keycode < ARRAYSIZE(s_KeyDown) ? !s_KeyDown[(uint32_t)keycode] && s_PreviousKeyDown[(uint32_t)keycode]: false);
	}

	bool Input::GetKeyDown(KeyCode keycode)
	{
		return ((uint32_t)keycode < ARRAYSIZE(s_KeyDown) ? s_KeyDown[(uint32_t)keycode] && !s_PreviousKeyDown[(uint32_t)keycode] : false);
	}

	bool Input::GetMouseButton(MouseButton button)
	{
		return ((uint32_t)button < ARRAYSIZE(s_KeyDown) ? s_KeyDown[(uint32_t)button] : false);
	}

	bool Input::GetMouseButtonUp(MouseButton button)
	{
		return ((uint32_t)button < ARRAYSIZE(s_KeyDown) ? !s_KeyDown[(uint32_t)button] && s_PreviousKeyDown[(uint32_t)button] : false);
	}

	bool Input::GetMouseButtonDown(MouseButton button)
	{
		return ((uint32_t)button < ARRAYSIZE(s_KeyDown) ? s_KeyDown[(uint32_t)button] && !s_PreviousKeyDown[(uint32_t)button] : false);
	}

	std::pair<int64_t, int64_t> Input::GetMousePos()
{
		if (GetCursorMode() == CursorMode::Hidden)
			return s_RelativeMousePos;
		else
			return s_MousePos;
	}

	CursorMode Input::GetCursorMode()
	{
		if (GetCursor() == 0)
			return CursorMode::Hidden;
		else
			return CursorMode::Visible;
	}

	InputMode Input::GetInputMode()
	{
		return s_InputMode;
	}

	void Input::SetCursorMode(CursorMode mode)
	{
		if (mode == CursorMode::Visible)
		{
			SetCursor(LoadCursor(GetModuleHandle(0), IDC_ARROW));
			SetMousePos(s_MousePos.first, s_MousePos.second + 25);
		}
		else
		{
			SetCursor(0);
		}
	}

	void Input::SetMousePos(int64_t x, int64_t y)
	{
		SetCursorPos(static_cast<int>(x), static_cast<int>(y));
	}

	void Input::SetInputMode(InputMode mode)
	{
		s_InputMode = mode;
	}

}
