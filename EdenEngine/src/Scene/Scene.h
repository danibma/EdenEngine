#pragma once

#include <entt/entt.hpp>

namespace Eden
{
	class Entity;
	class Scene
	{
		entt::registry m_Registry;
		friend class Entity;

	public:
		Scene() = default;

		Entity CreateEntity(const std::string_view name = "");
		void DeleteEntity(Entity& entity);

		template<typename... Components>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<Components...>();
		}

		size_t Size();
	};
}

