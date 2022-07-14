#pragma once

#include <entt/entt.hpp>
#include <filesystem>
#include <vector>

namespace Eden
{
	class Entity;
	class SceneSerializer;
	class Scene
	{
		friend class Entity;
		friend class SceneSerializer;


		entt::registry m_Registry;
		std::string m_Name = "Untitled";
		std::filesystem::path m_ScenePath = "";
		bool m_Loaded = false;
		/*
		 * Preparations work like a job system, we add a preparation to this vector and
		 * at the beginning of each frame we execute this preparations
		 * this way we dont modify or delete the resources during the frame
		 */
		std::vector<std::function<void()>> m_Preparations;
		entt::entity m_SelectedEntity = entt::null;

	public:
		Scene() = default;
		~Scene();

		Entity CreateEntity(const std::string_view name = "");
		void DeleteEntity(Entity& entity);

		void Clear();
		size_t Size();

		const std::string& GetName() { return m_Name; }

		void SetScenePath(const std::filesystem::path& path);
		const std::filesystem::path GetScenePath();

		void SetSceneLoaded(bool loaded);
		bool IsSceneLoaded();

		void ExecutePreparations();
		void AddPreparation(std::function<void()> preparation);

		Entity GetSelectedEntity();
		void SetSelectedEntity(Entity entity);

		template<typename... Components>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<Components...>();
		}

		Entity DuplicateEntity(Entity entity);
	};
}

