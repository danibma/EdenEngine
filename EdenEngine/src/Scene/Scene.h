#pragma once

#include <entt/entt.hpp>

namespace Eden
{
	class Entity;
	class SceneSerializer;
	class Scene
	{
		entt::registry m_Registry;
		friend class Entity;
		friend class SceneSerializer;

	public:
		Scene() = default;

		Entity CreateEntity(const std::string_view name = "");
		void DeleteEntity(Entity& entity);

		template<typename... Components>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<Components...>();
		}

		void Clear();

		size_t Size();
	};
}

