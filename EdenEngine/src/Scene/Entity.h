#pragma once

#include <entt/entt.hpp>

#include "Scene.h"
#include "Core/Base.h"

namespace Eden
{
	class Entity
	{
		entt::entity m_EntityHandler = { entt::null };
		Scene* m_Scene = nullptr;

	public:
		Entity() = default;
		Entity(entt::entity entity_handler, Scene* scene);

		// GetComponent, AddComponent, RemoveComponent, HasComponent
		template<typename T>
		T& GetComponent()
		{
			ED_ASSERT_LOG(HasComponent<T>(), "Entity does not have component");
			return m_Scene->m_Registry.get<T>(m_EntityHandler);
		}

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args)
		{
			ED_ASSERT_LOG(!HasComponent<T>(), "Entity already has component");
			return m_Scene->m_Registry.emplace<T>(m_EntityHandler, std::forward<Args>(args)...);
		}

		template<typename T>
		void RemoveComponent()
		{
			ED_ASSERT_LOG(HasComponent<T>(), "Entity does not have component");
			m_Scene->m_Registry.remove<T>(m_EntityHandler);
		}

		template<typename T>
		bool HasComponent()
		{
			return m_Scene->m_Registry.any_of<T>(m_EntityHandler);
		}

		bool Valid() const
		{
			if (m_Scene == nullptr)
				return false;
			return m_Scene->m_Registry.valid(m_EntityHandler);
		}

		bool operator==(const Entity& other) const
		{
			return m_EntityHandler == other.m_EntityHandler && m_Scene == other.m_Scene;
		}

		operator bool() const
		{
			return Valid();
		}

		operator entt::entity() { return m_EntityHandler; }
	};
}