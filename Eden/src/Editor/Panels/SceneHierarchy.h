#pragma once
#include <memory>

namespace Eden
{
	class Scene;
	class SceneHierarchy
	{
	public:
		bool bOpenHierarchy = true;
		bool bOpenInspector = true;

	public:
		SceneHierarchy();
		~SceneHierarchy();

		void Render();
		void EntityMenu();

	private:
		void DrawHierarchy();
		void EntityProperties();
		void DuplicateSelectedEntity();
		void DeleteSelectedEntity();

	};
}

