#include "Scene.h"
#include "Entity.h"
#include "Components.h"

namespace Eden
{
	Scene::~Scene()
	{
		auto entities = GetAllEntitiesWith<MeshComponent>();
		for (auto entity_id : entities)
		{
			Entity mesh_entity = { entity_id, this };
			mesh_entity.GetComponent<MeshComponent>().mesh_source->Destroy();
		}
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

	void Scene::SetSceneLoaded(bool loaded)
	{
		m_Loaded = loaded;
	}

	bool Scene::IsSceneLoaded()
	{
		return m_Loaded;
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

}
