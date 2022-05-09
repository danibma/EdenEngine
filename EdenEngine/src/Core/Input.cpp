#include "Input.h"

#include <windowsx.h>
#include "Log.h"

namespace Eden
{
	bool Input::m_keyDown[VK_OEM_CLEAR] = {};
	bool Input::m_previousKeyDown[VK_OEM_CLEAR] = {};
	std::pair<int64_t, int64_t> Input::m_mousePos;
	std::pair<int64_t, int64_t> Input::m_relativeMousePos;
	float Input::m_mouseScrollDelta;

	void Input::UpdateInput()
	{
		memcpy(m_previousKeyDown, m_keyDown, sizeof(m_keyDown));
	}

	void Input::HandleInput(uint32_t message, uint32_t code, uint32_t lParam)
	{
		switch (message)
		{
		case WM_KEYUP: case WM_SYSKEYUP:
			m_keyDown[code] = false;
			break;
		case WM_KEYDOWN: case WM_SYSKEYDOWN:
			m_keyDown[code] = true;
			break;
		case WM_LBUTTONDOWN:
			m_keyDown[VK_LBUTTON] = true;
			break;
		case WM_LBUTTONUP:
			m_keyDown[VK_LBUTTON] = false;
			break;
		case WM_RBUTTONDOWN:
			m_keyDown[VK_RBUTTON] = true;
			break;
		case WM_RBUTTONUP:
			m_keyDown[VK_RBUTTON] = false;
			break;
		case WM_INPUT:
			if (GetCursorMode() == CursorMode::Hidden)
			{
				RAWINPUT raw;
				UINT rawSize = sizeof(raw);

				const UINT resultData = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &rawSize, sizeof(RAWINPUTHEADER));
				if (raw.header.dwType == RIM_TYPEMOUSE)
				{
					m_relativeMousePos.first += raw.data.mouse.lLastX;
					m_relativeMousePos.second += raw.data.mouse.lLastY;
					ED_LOG_TRACE("{}x{}", m_relativeMousePos.first, m_relativeMousePos.second);
				}
			}
			break;
		case WM_MOUSEMOVE:
			if (GetCursorMode() == CursorMode::Visible)
			{
				m_mousePos.first = GET_X_LPARAM(lParam);
				m_mousePos.second = GET_Y_LPARAM(lParam);
			}
			break;
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL:
			m_mouseScrollDelta = GET_WHEEL_DELTA_WPARAM(code);
			break;
		}
	}

	bool Input::GetKey(KeyCode keyCode)
	{
		return ((uint32_t)keyCode < ARRAYSIZE(m_keyDown) ? m_keyDown[(uint32_t)keyCode] : false);
	}

	bool Input::GetKeyUp(KeyCode keyCode)
	{
		return ((uint32_t)keyCode < ARRAYSIZE(m_keyDown) ? !m_keyDown[(uint32_t)keyCode] && m_previousKeyDown[(uint32_t)keyCode]: false);
	}

	bool Input::GetKeyDown(KeyCode keyCode)
	{
		return ((uint32_t)keyCode < ARRAYSIZE(m_keyDown) ? m_keyDown[(uint32_t)keyCode] && !m_previousKeyDown[(uint32_t)keyCode] : false);
	}

	bool Input::GetMouseButton(MouseButton button)
	{
		return ((uint32_t)button < ARRAYSIZE(m_keyDown) ? m_keyDown[(uint32_t)button] : false);
	}

	bool Input::GetMouseButtonUp(MouseButton button)
	{
		return ((uint32_t)button < ARRAYSIZE(m_keyDown) ? !m_keyDown[(uint32_t)button] && m_previousKeyDown[(uint32_t)button] : false);
	}

	bool Input::GetMouseButtonDown(MouseButton button)
	{
		return ((uint32_t)button < ARRAYSIZE(m_keyDown) ? m_keyDown[(uint32_t)button] && !m_previousKeyDown[(uint32_t)button] : false);
	}

	std::pair<int64_t, int64_t> Input::GetMousePos(Window* window)
	{
		if (GetCursorMode() == CursorMode::Hidden)
		{
			SetCursorPos(window->GetWidth() / 2, window->GetHeight() / 2);
			return m_relativeMousePos;
		}
		else
		{
			return m_mousePos;
		}
	}

	CursorMode Input::GetCursorMode()
	{
		if (GetCursor() == 0)
			return CursorMode::Hidden;
		else
			return CursorMode::Visible;
	}

	void Input::SetCursorMode(CursorMode mode)
	{
		if (mode == CursorMode::Visible)
		{
			SetCursor(LoadCursor(GetModuleHandle(0), IDC_ARROW));
		}
		else
		{
			SetCursor(0);
		}
	}

	void Input::SetMousePos(int64_t x, int64_t y)
	{
		if (GetCursorMode() == CursorMode::Hidden)
		{
			m_relativeMousePos = { x, y };
		}
		else
		{
			m_mousePos = { x, y };
		}
		
	}

}
