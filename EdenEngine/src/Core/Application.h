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

		float deltaTime = 0.0f;
		float creationTime = 0.0f; // Time since the application creation

	private:
		static Application* s_Instance;

		Timer m_DeltaTimer;
		Timer m_CreationTimer;

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

