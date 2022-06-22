#pragma once

#undef UNICODE
#include <windows.h>
#include <functional>

namespace Eden
{
	class Window
	{
		HWND m_Handle = {};
		bool m_CloseRequested = false;
		bool m_IsMinimized = false;
		uint32_t m_Width, m_Height;
		std::function<void(uint32_t, uint32_t)> m_ResizeCallback = [](uint32_t, uint32_t) {};

	public:
		Window(const char* title, uint32_t width, uint32_t height);
		~Window();

		void UpdateEvents();
		void ResizeCallback();
		void Resize(uint32_t width, uint32_t height);
		void SetResizeCallback(std::function<void(uint32_t, uint32_t)> resize_callback);

		void CloseWasRequested()			{ m_CloseRequested = true; }
		void SetMinimized(const bool value)	{ m_IsMinimized = value; }
		
		bool IsCloseRequested()			{ return m_CloseRequested; }
		bool IsMinimized()				{ return m_IsMinimized; }

		HWND GetHandle() { return m_Handle; }
		uint32_t GetWidth() { return m_Width; }
		uint32_t GetHeight() { return m_Height; }
		float GetAspectRatio() { return (float)m_Width / (float)m_Height; }
	};
}

