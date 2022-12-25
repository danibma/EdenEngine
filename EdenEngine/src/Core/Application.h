#pragma once
#include <string>

#include "Profiling/Timer.h"
#include <memory>
#include "Editor/Editor.h"

namespace Eden
{
	class Window;
	class EdenEd;

	class Application
	{
		friend class EdenEd; // temporary until there's no Renderer class

	protected:
		Window* window;
		EdenEd* editor;

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
		void RequestClose();

		float GetDeltaTime() { return deltaTime; }
		float GetTimeSinceCreation() { return creationTime; }

		virtual void OnInit();
		virtual void OnUpdate();
		virtual void OnDestroy();
	};
}

