#include "SceneSerializer.h"
#include "Entity.h"
#include "Components.h"

#include <fstream>
#define YAML_CPP_STATIC_DEFINE
#include <yaml-cpp/yaml.h>

#include <glm/glm.hpp>

namespace YAML
{
	// glm::vec3
	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	// glm::vec4
	template<>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			return node;
		}

		static bool decode(const Node& node, glm::vec4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};
}

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
		std::string sceneName = filepath.stem().string();

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << sceneName;
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		auto entities = m_Scene->GetAllEntitiesWith<TagComponent>();
		// This loop is inversed so that it serializes the entities from 0 to ...
		int num_entities = static_cast<int>(entities.size() - 1);
		for (int i = num_entities; i >= 0; --i)
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

		m_Scene->m_Name = sceneName;
	}

	bool SceneSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		std::ifstream stream(filepath);
		std::stringstream strStream;
		strStream << stream.rdbuf();

		YAML::Node data = YAML::Load(strStream.str());
		if (!data["Scene"])
			return false;

		std::string sceneName = data["Scene"].as<std::string>();
		ED_LOG_INFO("Deserializing scene '{}'", sceneName);
		m_Scene->m_Name = sceneName;

		auto entities = data["Entities"];
		if (entities)
		{
			for (auto entity : entities)
			{
				uint32_t id = entity["Entity"].as<uint32_t>();

				std::string name;
				auto tagComponent = entity["TagComponent"];
				if (tagComponent)
					name = tagComponent["Tag"].as<std::string>();

				Entity deserializedEntity = m_Scene->CreateEntity(name);

				auto transform_component = entity["TransformComponent"];
				if (transform_component)
				{
					// Entities always have transform
					auto& tc = deserializedEntity.GetComponent<TransformComponent>();
					tc.translation = transform_component["Translation"].as<glm::vec3>();
					tc.rotation = transform_component["Rotation"].as<glm::vec3>();
					tc.scale = transform_component["Scale"].as<glm::vec3>();
				}

				auto meshComponent = entity["MeshComponent"];
				if (meshComponent)
				{
					auto& mc = deserializedEntity.AddComponent<MeshComponent>();
					mc.meshPath = meshComponent["File Path"].as<std::string>();
				}

				auto pointLightComponent = entity["PointLightComponent"];
				if (pointLightComponent)
				{
					auto& pl = deserializedEntity.AddComponent<PointLightComponent>();
					auto& transform = deserializedEntity.GetComponent<TransformComponent>();

					pl.position = glm::vec4(transform.translation, 1.0f);
					pl.color = pointLightComponent["Color"].as<glm::vec4>();
					pl.intensity = pointLightComponent["Intensity"].as<float>();
				}

				auto directional_lightComponent = entity["DirectionalLightComponent"];
				if (directional_lightComponent)
				{
					auto& dl = deserializedEntity.AddComponent<DirectionalLightComponent>();
					auto& transform = deserializedEntity.GetComponent<TransformComponent>();

					dl.direction = glm::vec4(transform.rotation, 1.0f);
					dl.intensity = directional_lightComponent["Intensity"].as<float>();
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
			out << YAML::Key << "File Path" << YAML::Value << mc.meshPath;

			out << YAML::EndMap; // MeshComponent
		}

		if (entity.HasComponent<PointLightComponent>())
		{
			out << YAML::Key << "PointLightComponent";
			out << YAML::BeginMap; // PointLightComponent

			auto& pl = entity.GetComponent<PointLightComponent>();
			out << YAML::Key << "Color" << YAML::Value << pl.color;
			out << YAML::Key << "Intensity" << YAML::Value << pl.intensity;

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