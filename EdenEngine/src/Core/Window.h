#pragma once

#undef UNICODE
#include <windows.h>
#include <string>

namespace Eden
{
	class Window
	{
		HWND m_Handle = {};
		bool m_CloseRequested = false;
		bool m_IsMinimized = false;
		uint32_t m_Width, m_Height;

	public:
		Window(const char* title, uint32_t width, uint32_t height);
		~Window();

		void UpdateEvents();
		void Resize(uint32_t width, uint32_t height);

		inline void CloseWasRequested()			{ m_CloseRequested = true; }
		inline void SetMinimized(bool value)	{ m_IsMinimized = value; }

		inline bool IsCloseRequested()			{ return m_CloseRequested; }
		inline bool IsMinimized()				{ return m_IsMinimized; }

		inline HWND GetHandle() { return m_Handle; }
		inline uint32_t GetWidth() { return m_Width; }
		inline uint32_t GetHeight() { return m_Height; }
	};
}

