#include "Entity.h"

namespace Eden
{

	Entity::Entity(entt::entity entity_handler, Scene* scene)
		: m_EntityHandler(entity_handler)
		, m_Scene(scene)
	{
	}

}
