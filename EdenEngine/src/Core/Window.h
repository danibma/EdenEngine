#pragma once

#undef UNICODE
#include <windows.h>
#include <string>

namespace Eden
{
	class Window
	{
		HWND m_handle = {};
		bool m_closeRequested = false;
		bool m_isMinimized = false;
		uint32_t m_width, m_height;

	public:
		Window(const char* title, uint32_t width, uint32_t height);
		~Window();

		void UpdateEvents();
		void Resize(uint32_t width, uint32_t height);

		inline void CloseWasRequested()			{ m_closeRequested = true; }
		inline void SetMinimized(bool value)	{ m_isMinimized = value; }

		inline bool IsCloseRequested()			{ return m_closeRequested; }
		inline bool IsMinimized()				{ return m_isMinimized; }

		inline HWND GetHandle() { return m_handle; }
		inline uint32_t GetWidth() { return m_width; }
		inline uint32_t GetHeight() { return m_height; }
	};
}

