#pragma once
#include <memory>
#include "Scene\Scene.h"
#include "Graphics\D3D12RHI.h"

namespace Eden
{
	class SceneHierarchy
	{
	private:
		Scene* m_CurrentScene;
		D3D12RHI* m_RHI;

	public:
		bool open_scene_hierarchy = true;
		bool open_entity_properties = true;

	public:
		SceneHierarchy() = default;
		SceneHierarchy(D3D12RHI* rhi, Scene* current_scene);
		~SceneHierarchy();

		void Render();
		void EntityMenu();
		void SetCurrentScene(Scene* current_scene);

	private:
		void DrawHierarchy();
		void EntityProperties();

	};
}

