#pragma once

#include <stdint.h>

#include "Window.h"
#include "KeyCodes.h"

namespace Eden
{
	enum class CursorMode
	{
		Hidden	= 0,
		Visible	= 1
	};

	enum class InputMode
	{
		Game = 0,
		UI
	};

	class Input
	{
		static bool s_KeyDown[VK_OEM_CLEAR];
		static bool s_PreviousKeyDown[VK_OEM_CLEAR];
		static std::pair<int64_t, int64_t> s_MousePos;
		static std::pair<int64_t, int64_t> s_RelativeMousePos;
		static float s_MouseScrollDelta;
		static InputMode s_InputMode;

	public:
		static void UpdateInput();
		static void HandleInput(uint64_t message, uint64_t code, uint64_t lParam);

		static void SetCursorMode(CursorMode mode);
		static void SetMousePos(int64_t x, int64_t y);
		static void SetInputMode(InputMode mode);

		/*
		* Returns true while the user holds down the key.
		*/
		static bool GetKey(KeyCode keycode);

		/*
		* Returns true during the frame the user releases the key.
		*/
		static bool GetKeyUp(KeyCode keycode);

		/*
		* Returns true during the frame the user starts pressing down the key.
		*/
		static bool GetKeyDown(KeyCode keycode);

		/*
		* Returns true while the user holds down the mouse button.
		*/
		static bool GetMouseButton(MouseButton button);

		/*
		* Returns true during the frame the user releases the mouse button.
		*/
		static bool GetMouseButtonUp(MouseButton button);

		/*
		* Returns true during the frame the user starts pressing down the mouse button.
		*/
		static bool GetMouseButtonDown(MouseButton button);

		/*
		* Returns the current mouse position
		*/
		static std::pair<int64_t, int64_t> GetMousePos();

		/*
		* Returns the mouse scroll delta
		*/
		static float GetMouseScrollDelta() { return s_MouseScrollDelta; }

		/*
		* Returns wether the cursor is being shown or is hidden
		*/
		static CursorMode GetCursorMode();

		static InputMode GetInputMode();
	};
}
