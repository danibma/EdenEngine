#include "Application.h"
#include "Log.h"
#include "Window.h"
#include "Graphics/D3D12/D3D12RHI.h"
#include "Graphics/RHI.h"
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
	#else
		window = enew Window("Eden Engine", 1600, 900);
	#endif 

		rhi = std::make_shared<D3D12RHI>();
		GfxResult error = rhi->Init(window);
		ensureMsg(error == GfxResult::kNoError, "Failed to initialize RHI");

		m_CreationTimer.Record();
		s_Instance = this;
	}

	Application::~Application()
	{
		rhi->Shutdown();
		edelete window;
		Log::Shutdown();
	}

	void Application::Run()
	{
		OnInit();

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
				OnUpdate();
			}
		}

		OnDestroy();
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
		std::string newTitle = title + " - " + window->GetDefaultTitle() + " " + Utils::APIToString(rhi->GetCurrentAPI()); // NOTE(Daniel): in case of adding more API's make this dynamic
		SetWindowTextA(window->GetHandle(), newTitle.c_str());
	}

	void Application::OnInit()
	{
	}

	void Application::OnUpdate()
	{
	}

	void Application::OnDestroy()
	{
	}

}