#include "Core/Window.h"
#include "Core/Memory.h"
#include "Core/Input.h"
#include "Core/Log.h"
#include <stdio.h>

using namespace Eden;

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	Log::Init();

	Window* window = enew Window("Eden Engine", 1600, 900);

	while (!window->IsCloseRequested())
	{
		window->UpdateEvents();

		if (!window->IsMinimized())
		{
			if (Input::GetKeyDown(KeyCode::A))
			{
				ED_LOG_INFO("Key Pressed!");
			}
			else if(Input::GetKeyUp(KeyCode::A))
			{
				ED_LOG_INFO("Key Up!");
			}
		}
	}

	edelete window;

	Log::Shutdown();

	return 0;
}

