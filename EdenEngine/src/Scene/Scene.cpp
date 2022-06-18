#include "Scene.h"
#include "Entity.h"
#include "Components.h"

namespace Eden
{

	Entity Scene::CreateEntity(const std::string_view name /* = "" */)
	{
		Entity entity = { m_Registry.create(), this };
		auto& tag = entity.AddComponent<TagComponent>();
		tag.tag = name.empty() ? "Unnamed Entity" : name;

		return entity;
	}

	size_t Scene::Size()
	{
		return m_Registry.size();
	}

}