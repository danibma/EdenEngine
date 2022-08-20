#pragma once

#undef UNICODE
#include <windows.h>
#include <functional>

namespace Eden
{
	class Window
	{
		HWND m_Handle = {};
		bool m_bIsCloseRequested = false;
		bool m_bIsMinimized = false;
		uint32_t m_Width, m_Height;
		std::string m_DefaultTitle;
		std::function<void(uint32_t, uint32_t)> m_ResizeCallback = [](uint32_t, uint32_t) {};

	public:
		Window(const char* title, uint32_t width, uint32_t height);
		~Window();

		void UpdateEvents();
		void ResizeCallback();
		void Resize(uint32_t width, uint32_t height);
		void SetResizeCallback(std::function<void(uint32_t, uint32_t)> resizeCallback);

		void CloseWasRequested()			{ m_bIsCloseRequested = true; }
		void SetMinimized(const bool value)	{ m_bIsMinimized = value; }
		
		bool IsCloseRequested()			{ return m_bIsCloseRequested; }
		bool IsMinimized()				{ return m_bIsMinimized; }

		HWND GetHandle() { return m_Handle; }
		uint32_t GetWidth() { return m_Width; }
		uint32_t GetHeight() { return m_Height; }
		float GetAspectRatio() { return (float)m_Width / (float)m_Height; }
		const std::string& GetDefaultTitle() const { return m_DefaultTitle; }
	};
}

