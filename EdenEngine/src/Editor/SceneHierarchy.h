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
		bool open_scene_hierarchy = true;
		bool open_entity_properties = true;

	public:
		SceneHierarchy() = default;
		SceneHierarchy(std::shared_ptr<IRHI>& rhi, Scene* current_scene);
		~SceneHierarchy();

		void Render();
		void EntityMenu();
		void SetCurrentScene(Scene* current_scene);

	private:
		void DrawHierarchy();
		void EntityProperties();

	};
}

