#pragma once
#include <string>

#include "Profiling/Timer.h"
#include "Core/Delegates.h"
#include "Window.h"

#include <memory>

namespace Eden
{
	struct ApplicationDescription
	{
		std::string Title;
		uint32_t	Width, Height;
	};

	class Application
	{
	private:
		static Application* s_Instance;

		Window* m_Window;

		float m_DeltaTime = 0.0f;
		float m_CreationTime = 0.0f; // Time since the application creation

		Timer m_DeltaTimer;
		Timer m_CreationTimer;

	public:
		DECLARE_MULTICAST_DELEGATE(AppUpdate);
		AppUpdate updateDelegate;

	public:
		Application(const ApplicationDescription& description);
		~Application();

		void Run();

		std::string OpenFileDialog(const char* filter = "");
		std::string SaveFileDialog(const char* filter = "");

		static Application* Get();

		void ChangeWindowTitle(const std::string& title);
		const std::string& GetWindowTitle() const { return m_Window->GetTitle(); }
		void RequestClose();
		void MaximizeWindow();
		void MinimizeWindow();
		bool IsWindowMaximized();

		float GetDeltaTime() { return m_DeltaTime; }
		float GetTimeSinceCreation() { return m_CreationTime; }

		Window* GetWindow() { return m_Window; }
	};
}

