#pragma once

#include "Scene.h"

#include <string>
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
	enum class EdenExtension
	{
		kNone = 0,
		kScene,
		kModel,
		kEnvironmentMap
	};

	namespace Utils
	{
		inline EdenExtension StringToExtension(const std::string& ext)
		{
			if (ext == ".escene")
				return EdenExtension::kScene;
			else if (ext == ".gltf" || ext == ".glb")
				return EdenExtension::kModel;
			else if (ext == ".hdr")
				return EdenExtension::kEnvironmentMap;
			else
				return EdenExtension::kNone;
		}

		inline std::string ExtensionToString(EdenExtension ext)
		{
			switch (ext)
			{
			case EdenExtension::kScene:
				return ".escene";
			case EdenExtension::kModel:
				return ".glb";
			case EdenExtension::kEnvironmentMap:
				return ".hdr";
			default:
				return "";
			}
		}
	}

	class SceneSerializer
	{
		Scene* m_Scene;

	public:
		SceneSerializer() = default;
		SceneSerializer(Scene* scene);

		void Serialize(const std::filesystem::path& filepath);
		bool Deserialize(const std::filesystem::path& filepath);

	private:
		void SerializeEntity(YAML::Emitter& out, Entity entity);
	};
}

