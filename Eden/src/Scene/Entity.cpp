#include "Entity.h"

namespace Eden
{

	Entity::Entity(entt::entity entityHandler, Scene* scene)
		: m_EntityHandler(entityHandler)
		, m_Scene(scene)
	{
	}

}
