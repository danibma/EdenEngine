#include "SceneSerializer.h"
#include "Entity.h"
#include "Components.h"

#include <filesystem>
#include "entt\entt.hpp"

namespace Eden
{
	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

	SceneSerializer::SceneSerializer(Scene* scene)
		:m_Scene(scene)
	{
		if (!std::filesystem::exists("assets/Scenes"))
			std::filesystem::create_directories("assets/Scenes");
	}

	void SceneSerializer::Serialize(const std::filesystem::path& filepath)
	{
		std::string scene_name = filepath.stem().string();

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << scene_name;
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		auto entities = m_Scene->GetAllEntitiesWith<TagComponent>();
		// This loop is inversed so that it serializes the entities from 0 to ...
		for (int i = entities.size() - 1; i >= 0; --i)
		{
			Entity entity = { entities[i], m_Scene};
			if (!entity)
				return;

			SerializeEntity(out, entity);
		}
		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();

		m_Scene->m_Name = scene_name;
	}

	bool SceneSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		std::ifstream stream(filepath);
		std::stringstream str_stream;
		str_stream << stream.rdbuf();

		YAML::Node data = YAML::Load(str_stream.str());
		if (!data["Scene"])
			return false;

		std::string scene_name = data["Scene"].as<std::string>();
		ED_LOG_INFO("Deserializing scene '{}'", scene_name);
		m_Scene->m_Name = scene_name;

		auto entities = data["Entities"];
		if (entities)
		{
			for (auto entity : entities)
			{
				uint32_t id = entity["Entity"].as<uint32_t>();

				std::string name;
				auto tag_component = entity["TagComponent"];
				if (tag_component)
					name = tag_component["Tag"].as<std::string>();

				ED_LOG_INFO("Deserialized entity with ID = {}, name = {}", id, name);

				Entity deserialized_entity = m_Scene->CreateEntity(name);

				auto transform_component = entity["TransformComponent"];
				if (transform_component)
				{
					// Entities always have transform
					auto& tc = deserialized_entity.GetComponent<TransformComponent>();
					tc.translation = transform_component["Translation"].as<glm::vec3>();
					tc.rotation = transform_component["Rotation"].as<glm::vec3>();
					tc.scale = transform_component["Scale"].as<glm::vec3>();
				}

				auto mesh_component = entity["MeshComponent"];
				if (mesh_component)
				{
					auto& mc = deserialized_entity.AddComponent<MeshComponent>();
					mc.mesh_path = mesh_component["File Path"].as<std::string>();
				}

				auto point_light_component = entity["PointLightComponent"];
				if (point_light_component)
				{
					auto& pl = deserialized_entity.AddComponent<PointLightComponent>();
					auto& transform = deserialized_entity.GetComponent<TransformComponent>();

					pl.position = glm::vec4(transform.translation, 1.0f);
					pl.color = point_light_component["Color"].as<glm::vec4>();
					pl.constant_value = point_light_component["Constant Value"].as<float>();
					pl.quadratic_value = point_light_component["Quadratic Value"].as<float>();
					pl.linear_value = point_light_component["Linear Value"].as<float>();
				}

				auto directional_light_component = entity["DirectionalLightComponent"];
				if (directional_light_component)
				{
					auto& dl = deserialized_entity.AddComponent<DirectionalLightComponent>();
					auto& transform = deserialized_entity.GetComponent<TransformComponent>();

					dl.direction = glm::vec4(transform.rotation, 1.0f);
					dl.intensity = directional_light_component["Intensity"].as<float>();
				}
			}
		}

		return true;
	}

	void SceneSerializer::SerializeEntity(YAML::Emitter& out, Entity entity)
	{
		out << YAML::BeginMap; // Entity
		out << YAML::Key << "Entity" << YAML::Value << entity.GetID();

		if (entity.HasComponent<TagComponent>())
		{
			out << YAML::Key << "TagComponent";
			out << YAML::BeginMap; // TagComponent

			auto& tag = entity.GetComponent<TagComponent>().tag;
			out << YAML::Key << "Tag" << YAML::Value << tag;

			out << YAML::EndMap; // TagComponent
		}

		if (entity.HasComponent<TransformComponent>())
		{
			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap; // TransformComponent

			auto& tc = entity.GetComponent<TransformComponent>();
			out << YAML::Key << "Translation" << YAML::Value << tc.translation;
			out << YAML::Key << "Rotation" << YAML::Value << tc.rotation;
			out << YAML::Key << "Scale" << YAML::Value << tc.scale;

			out << YAML::EndMap; // TransformComponent
		}

		if (entity.HasComponent<MeshComponent>())
		{
			out << YAML::Key << "MeshComponent";
			out << YAML::BeginMap; // MeshComponent

			auto& mc = entity.GetComponent<MeshComponent>();
			out << YAML::Key << "File Path" << YAML::Value << mc.mesh_path;

			out << YAML::EndMap; // MeshComponent
		}

		if (entity.HasComponent<PointLightComponent>())
		{
			out << YAML::Key << "PointLightComponent";
			out << YAML::BeginMap; // PointLightComponent

			auto& pl = entity.GetComponent<PointLightComponent>();
			out << YAML::Key << "Color" << YAML::Value << pl.color;
			out << YAML::Key << "Constant Value" << YAML::Value << pl.constant_value;
			out << YAML::Key << "Linear Value" << YAML::Value << pl.linear_value;
			out << YAML::Key << "Quadratic Value" << YAML::Value << pl.quadratic_value;

			out << YAML::EndMap; // PointLightComponent
		}

		if (entity.HasComponent<DirectionalLightComponent>())
		{
			out << YAML::Key << "DirectionalLightComponent";
			out << YAML::BeginMap; // DirectionalLightComponent

			auto& dl = entity.GetComponent<DirectionalLightComponent>();
			out << YAML::Key << "Intensity" << YAML::Value << dl.intensity;

			out << YAML::EndMap; // DirectionalLightComponent
		}

		out << YAML::EndMap; // Entity
	}

}