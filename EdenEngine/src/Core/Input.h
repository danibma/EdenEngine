#pragma once

#include <windows.h>
#include <stdint.h>
#include <functional>

namespace Eden
{
	enum class KeyCode
	{
		// Keyboard
		Backspace			= 0x08,
		Tab					= 0x09,
		Enter				= 0x0D,
		LeftShift			= 0xA0,
		RightShift			= 0xA1,
		Shift				= 0x10,	
		LeftControl			= 0xA2,
		RightControl		= 0xA3,
		Control				= 0x11,	
		CapsLock			= 0x14,
		Escape				= 0x1B,
		Space				= 0x20,
		PageDown			= 0x21,
		PageUp				= 0x22,
		End					= 0x23,
		Home				= 0x24,
		LeftArrow			= 0x25,
		UpArrow				= 0x26,
		RightArrow			= 0x27,
		DownArrow			= 0x28,
		Print				= 0x2A,
		PrintScreen			= 0x2C,
		Insert				= 0x2D,
		Delete				= 0x2E,
		LeftWindows			= 0x5B,
		RightWindows		= 0x5C,
		Multiply			= 0x6A,
		Add					= 0x6B,
		Subtract			= 0x6D,
		Divide				= 0x6F,
		NumLock				= 0x90,
		ScrollLock			= 0x91,


		// Numbers
		Zero				= 0x30,
		One					= 0x31,
		Two					= 0x32,
		Three				= 0x33,
		Four				= 0x34,
		Five				= 0x35,
		Six					= 0x36,
		Seven				= 0x37,
		Eight				= 0x38,
		Nine				= 0x39,

		// KeyPad
		KeyPad0			= 0x60,
		KeyPad1			= 0x61,
		KeyPad2			= 0x62,
		KeyPad3			= 0x63,
		KeyPad4			= 0x64,
		KeyPad5			= 0x65,
		KeyPad6			= 0x66,
		KeyPad7			= 0x67,
		KeyPad8			= 0x68,
		KeyPad9			= 0x69,

		// Letters
		A					= 0x41,
		B					= 0x42,
		C					= 0x43,
		D					= 0x44,
		E					= 0x45,
		F					= 0x46,
		G					= 0x47,
		H					= 0x48,
		I					= 0x49,
		J					= 0x4A,
		K					= 0x4B,
		L					= 0x4C,
		M					= 0x4D,
		N					= 0x4E,
		O					= 0x4F,
		P					= 0x50,
		Q					= 0x51,
		R					= 0x52,
		S					= 0x53,
		T					= 0x54,
		U					= 0x55,
		V					= 0x56,
		W					= 0x57,
		X					= 0x58,
		Y					= 0x59,
		Z					= 0x5A,

		// Function keys
		F1				= 0x70,
		F2				= 0x71,
		F3				= 0x72,
		F4				= 0x73,
		F5				= 0x74,
		F6				= 0x75,
		F7				= 0x76,
		F8				= 0x77,
		F9				= 0x78,
		F10				= 0x79,
		F11				= 0x7A,
		F12				= 0x7B,
		F13				= 0x7C,
		F14				= 0x7D,
		F15				= 0x7E,
		F16				= 0x7F,
		F17				= 0x80,
		F18				= 0x81,
		F19				= 0x82,
		F20				= 0x83,
		F21				= 0x84,
		F22				= 0x85,
		F23				= 0x86,
		F24				= 0x87,
	};

	enum class MouseButton
	{
		// Mouse Buttons
		LeftButton = 0x01,
		RightButton = 0x02,
		MiddleButton = 0x04,
	};

	enum class CursorMode
	{
		Hidden	= 0,
		Visible	= 1
	};

	class Input
	{
		static bool m_keyDown[VK_OEM_CLEAR];
		static bool m_previousKeyDown[VK_OEM_CLEAR];
		static std::pair<uint32_t, uint32_t> m_MousePos;
		static float m_MouseScrollDelta;

	public:
		static void UpdateInput();
		static void HandleInput(uint32_t message, uint32_t code, uint32_t lParam);

		static void SetCursorMode(CursorMode mode);

		/*
		* Returns whether the given key is held down.
		*/
		static bool GetKey(KeyCode keyCode);

		/*
		* Returns true during the frame the user releases the key.
		*/
		static bool GetKeyUp(KeyCode keyCode);

		/*
		* Returns true while the user holds down the key.
		*/
		static bool GetKeyDown(KeyCode keyCode);

		/*
		* Returns whether the given mouse button is held down.
		*/
		static bool GetMouseButton(MouseButton button);

		/*
		* Returns true during the frame the user releases the mouse button.
		*/
		static bool GetMouseButtonUp(MouseButton button);

		/*
		* Returns true while the user holds down the mouse button.
		*/
		static bool GetMouseButtonDown(MouseButton button);

		/*
		* Returns the current mouse position
		*/
		static std::pair<uint32_t, uint32_t> GetMousePos() { return m_MousePos; }

		/*
		* Returns the mouse scroll delta
		*/
		static float GetMouseScrollDelta() { return m_MouseScrollDelta; }

		/*
		* Returns wether the cursor is being shown or is hidden
		*/
		static CursorMode GetCursorMode();
	};
}
