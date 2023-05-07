#include "Application.h"
#include "Log.h"
#include "Window.h"
#include "Renderer/Renderer.h"
#include "Memory.h"
#include "Utilities/Utils.h"

namespace Eden
{
	Application* Application::s_Instance;

	Application::Application(const ApplicationDescription& description)
	{
		Log::Init();

		m_Window = enew Window(description.Title.c_str(), description.Width, description.Height);

		m_CreationTimer.Record();
		s_Instance = this;
	}

	Application::~Application()
	{
		edelete m_Window;

		Log::Shutdown();
	}

	void Application::Run()
	{
		while (!m_Window->IsCloseRequested())
		{
			m_Window->UpdateEvents();

			// Update timers
			m_DeltaTime = m_DeltaTimer.ElapsedSeconds();
			m_DeltaTimer.Record();
			m_CreationTime = m_CreationTimer.ElapsedSeconds();

			if (!m_Window->IsMinimized())
			{
				updateDelegate.Broadcast();
			}
		}
	}

	std::string Application::OpenFileDialog(const char* filter /*= ""*/)
	{
		return Utils::OpenFileDialog(m_Window->GetHandle(), filter);
	}

	std::string Application::SaveFileDialog(const char* filter /*= ""*/)
	{
		return Utils::SaveFileDialog(m_Window->GetHandle(), filter);
	}

	Application* Application::Get()
	{
		return s_Instance;
	}

	void Application::ChangeWindowTitle(const std::string& title)
	{
		m_Window->ChangeTitle(title);
	}

	void Application::RequestClose()
	{
		m_Window->CloseWasRequested();
	}

	void Application::MaximizeWindow()
	{
		m_Window->Maximize();
	}

	void Application::MinimizeWindow()
	{
		m_Window->Minimize();
	}

	bool Application::IsWindowMaximized()
	{
		return m_Window->IsMaximized();
	}
}