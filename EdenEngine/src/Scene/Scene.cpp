#include "Scene.h"
#include "Entity.h"
#include "Components.h"

namespace Eden
{
	Scene::~Scene()
	{
		auto entities = GetAllEntitiesWith<MeshComponent>();
		for (auto entityId : entities)
		{
			Entity meshEntity = { entityId, this };
			meshEntity.GetComponent<MeshComponent>().meshSource->Destroy();
		}

		m_Preparations.clear();
	}

	Entity Scene::CreateEntity(const std::string_view name /* = "" */)
	{
		Entity entity = { m_Registry.create(), this };
		auto& tag = entity.AddComponent<TagComponent>();
		tag.tag = name.empty() ? "Empty Entity" : name;
		entity.AddComponent<TransformComponent>();

		return entity;
	}

	void Scene::DeleteEntity(Entity& entity)
	{
		m_Registry.destroy(entity);
		m_SelectedEntity = static_cast<entt::entity>((uint32_t)(m_SelectedEntity) - 1);
	}

	void Scene::Clear()
	{
		m_Registry.clear<TransformComponent>();
	}

	size_t Scene::Size()
	{
		return m_Registry.size();
	}

	void Scene::SetScenePath(const std::filesystem::path& path)
	{
		m_ScenePath = path;
	}

	const std::filesystem::path Scene::GetScenePath()
	{
		return m_ScenePath;
	}

	void Scene::SetSceneLoaded(bool bWantToLoad)
	{
		m_bIsSceneLoaded = bWantToLoad;
	}

	bool Scene::IsSceneLoaded()
	{
		return m_bIsSceneLoaded;
	}

	void Scene::ExecutePreparations()
	{
		for (auto preparation : m_Preparations)
			preparation();

		m_Preparations.clear();
	}

	void Scene::AddPreparation(std::function<void()> preparation)
	{
		m_Preparations.emplace_back(preparation);
	}

	Entity Scene::GetSelectedEntity()
	{
		return { m_SelectedEntity, this };
	}

	void Scene::SetSelectedEntity(Entity entity)
	{
		m_SelectedEntity = entity;
	}

	Entity Scene::DuplicateEntity(Entity entity)
	{
		std::string name = entity.GetComponent<TagComponent>().tag;
		auto newEntity = CreateEntity(name);
		entity.CopyComponentIfExists<TransformComponent>(newEntity);
		entity.CopyComponentIfExists<MeshComponent>(newEntity);
		entity.CopyComponentIfExists<PointLightComponent>(newEntity);
		entity.CopyComponentIfExists<DirectionalLightComponent>(newEntity);

		return newEntity;
	}
}
