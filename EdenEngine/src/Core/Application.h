#pragma once
#include <string>

#include "Profiling/Timer.h"

namespace Eden
{
	class Window;
	class D3D12RHI;

	class Application
	{
	protected:
		Window* window;
		D3D12RHI* rhi;

		// Timer stuff
		Timer timer;
		float delta_time = 0.0f;

	public:
		Application();
		~Application();

		void Run();
		std::string OpenFileDialog(const char* filter = "");
		std::string SaveFileDialog(const char* filter = "");

		virtual void OnInit();
		virtual void OnUpdate();
		virtual void OnDestroy();
	};
}

