#pragma once
#include <string>

#include "Profiling/Timer.h"
#include <memory>

namespace Eden
{
	class Window;
	class IRHI;

	class Application
	{
	protected:
		Window* window;
		std::shared_ptr<IRHI> rhi;

		// Timer stuff
		Timer timer;
		float delta_time = 0.0f;

	private:
		static Application* s_Instance;

	public:
		Application();
		~Application();

		void Run();

		std::string OpenFileDialog(const char* filter = "");
		std::string SaveFileDialog(const char* filter = "");

		static Application* Get();

		void ChangeWindowTitle(const std::string& title);

		virtual void OnInit();
		virtual void OnUpdate();
		virtual void OnDestroy();
	};
}

