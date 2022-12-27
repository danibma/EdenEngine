#include "Application.h"
#include "Log.h"
#include "Window.h"
#include "Graphics/Renderer.h"
#include "Memory.h"
#include "Profiling/Profiler.h"
#include "Utilities/Utils.h"

namespace Eden
{
	Application* Application::s_Instance;

	Application::Application()
	{
		Log::Init();

	#ifdef ED_DEBUG
		window = enew Window("Eden Engine[DEBUG]", 1600, 900);
	#elif defined(ED_PROFILING)
		window = enew Window("Eden Engine[Profiling]", 1600, 900);
	#else
		window = enew Window("Eden Engine", 1600, 900);
	#endif 

		Renderer::Init(window);

#ifdef WITH_EDITOR
		editor = new EdenEd();
		editor->Init(window);
#endif // WITH_EDITOR

		m_CreationTimer.Record();
		s_Instance = this;
	}

	Application::~Application()
	{
#ifdef WITH_EDITOR
		editor->Shutdown();
		edelete editor;
#endif // WITH_EDITOR

		Renderer::Shutdown();

		edelete window;

		Log::Shutdown();
	}

	void Application::Run()
	{
		while (!window->IsCloseRequested())
		{
			ED_PROFILE_FRAME("MainThread");

			window->UpdateEvents();

			// Update timers
			deltaTime = m_DeltaTimer.ElapsedSeconds();
			m_DeltaTimer.Record();
			creationTime = m_CreationTimer.ElapsedSeconds();

			if (!window->IsMinimized())
			{
				Renderer::BeginRender();
				Renderer::Render();
#ifdef WITH_EDITOR
				editor->Update();
#endif
				Renderer::EndRender();
			}
		}
	}

	std::string Application::OpenFileDialog(const char* filter /*= ""*/)
	{
		return Utils::OpenFileDialog(window->GetHandle(), filter);
	}

	std::string Application::SaveFileDialog(const char* filter /*= ""*/)
	{
		return Utils::SaveFileDialog(window->GetHandle(), filter);
	}

	Application* Application::Get()
	{
		return s_Instance;
	}

	void Application::ChangeWindowTitle(const std::string& title)
	{
		std::string newTitle = title + " - " + window->GetDefaultTitle() + " " + Utils::APIToString(Renderer::GetCurrentAPI()); // NOTE(Daniel): in case of adding more API's make this dynamic
		SetWindowTextA(window->GetHandle(), newTitle.c_str());
	}

	void Application::RequestClose()
	{
		window->CloseWasRequested();
	}
}