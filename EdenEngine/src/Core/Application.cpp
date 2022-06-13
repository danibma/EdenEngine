#include "Application.h"
#include "Log.h"
#include "Window.h"
#include "Graphics/D3D12RHI.h"
#include "Memory.h"
#include "Profiling/Profiler.h"

namespace Eden
{

	Application::Application()
	{
		Log::Init();

	#ifdef ED_DEBUG
		window = enew Window("Eden Engine[DEBUG]", 1600, 900);
	#else
		window = enew Window("Eden Engine", 1600, 900);
	#endif 

		rhi = enew D3D12RHI(window);
		rhi->EnableImGui();
	}

	Application::~Application()
	{
		edelete rhi;
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
			delta_time = (float)timer.ElapsedSeconds();
			timer.Record();

			if (!window->IsMinimized())
			{
				OnUpdate();
			}
		}

		OnDestroy();
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