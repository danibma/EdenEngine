#include "Input.h"

#include <windowsx.h>

namespace Eden
{
	bool Input::m_keyDown[VK_OEM_CLEAR] = {};
	bool Input::m_previousKeyDown[VK_OEM_CLEAR] = {};
	std::pair<uint32_t, uint32_t> Input::m_mousePos;
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
			m_keyDown[VK_RBUTTON] = true;
			break;
		case WM_MOUSEMOVE:
			m_mousePos.first  = GET_X_LPARAM(lParam);
			m_mousePos.second = GET_Y_LPARAM(lParam);
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

	CursorMode Input::GetCursorMode()
	{
		CURSORINFO cursorInfo;
		cursorInfo.cbSize = sizeof(CURSORINFO); // Note that you must set the cbSize member to sizeof(CURSORINFO) before calling this function.
		GetCursorInfo(&cursorInfo);

		if (cursorInfo.hCursor == 0)
			return CursorMode::Hidden;
		else
			return CursorMode::Visible;
	}

	void Input::SetCursorMode(CursorMode mode)
	{
		ShowCursor((uint32_t)mode);
	}
}