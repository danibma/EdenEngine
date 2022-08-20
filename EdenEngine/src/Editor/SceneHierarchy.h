#pragma once
#include <memory>

namespace Eden
{
	class Scene;
	class IRHI;
	class SceneHierarchy
	{
	private:
		Scene* m_CurrentScene;
		std::shared_ptr<IRHI> m_RHI;

	public:
		bool bOpenHierarchy = true;
		bool bOpenInspector = true;

	public:
		SceneHierarchy() = default;
		SceneHierarchy(std::shared_ptr<IRHI>& rhi, Scene* currentScene);
		~SceneHierarchy();

		void Render();
		void EntityMenu();
		void SetCurrentScene(Scene* currentScene);

	private:
		void DrawHierarchy();
		void EntityProperties();
		void DuplicateSelectedEntity();
		void DeleteSelectedEntity();

	};
}

