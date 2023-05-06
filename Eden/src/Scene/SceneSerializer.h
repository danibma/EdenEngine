#pragma once

#include "Scene.h"

namespace YAML
{
	class Emitter;
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

