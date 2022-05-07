#include "Core/Window.h"
#include "Core/Memory.h"
#include "Core/Input.h"
#include "Core/Log.h"
#include "Profiling/Timer.h"
#include "Profiling/Profiler.h"

#include <algorithm>

using namespace Eden;

Window* window;

// Timer stuff
Timer timer;
float deltaTime;

void Init()
{
	Log::Init();

	window = enew Window("Eden Engine", 1600, 900);
}

void Update()
{
	ED_PROFILE_FUNCTION();

	window->UpdateEvents();

	// Update timers
	deltaTime = (float)timer.ElapsedSeconds();
	timer.Record();

	if (!window->IsMinimized())
	{
		if (Input::GetKeyDown(KeyCode::A))
		{
			ED_LOG_INFO("Key Pressed!");
		}
		else if (Input::GetKeyUp(KeyCode::A))
		{
			ED_LOG_INFO("Key Up!");
		}
	}
}

void Destroy()
{
	edelete window;

	Log::Shutdown();
}


int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	Init();

	while (!window->IsCloseRequested())
	{
		ED_PROFILE_FRAME("MainThread");

		Update();
	}

	Destroy();

	return 0;
}

