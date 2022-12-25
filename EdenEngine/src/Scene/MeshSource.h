#pragma once

#include "Graphics/RHI.h"

#include <functional>
#include <vector>
#include <filesystem>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <tinygltf/tiny_gltf.h>

namespace Eden
{
	struct VertexData
	{
		glm::vec3 position;
		glm::vec2 uv;
		glm::vec3 normal;
		glm::vec4 color;

		bool operator==(const VertexData& other) const
		{
			return position == other.position && uv == other.uv && normal == other.normal;
		}
	};

	struct MeshSource
	{
		struct Mesh
		{
			struct SubMesh
			{
				Texture diffuseTexture;
				Texture emissiveTexture;
				uint32_t vertexStart;
				uint32_t indexStart;
				uint32_t indexCount;
			};

			std::vector<std::shared_ptr<SubMesh>> submeshes;
			glm::mat4 gltfMatrix = glm::mat4(1.0f);
		};

		uint32_t vertexCount;
		uint32_t indexCount;
		Buffer meshVb;
		Buffer meshIb;
		std::vector<std::shared_ptr<Mesh>> meshes;
		bool bHasMesh = false;
		bool bIsTextured = false;

		MeshSource() = default;
		void LoadGLTF(std::filesystem::path file);
		void Destroy();

		~MeshSource()
		{
			Destroy();
		}

	private:
		void LoadImage(Texture* texture, tinygltf::Model& gltfModel, int32_t imageIndex);
	};
}

template<> struct std::hash<Eden::VertexData> {
	size_t operator()(Eden::VertexData const& vertex) const noexcept
	{
		return (hash<glm::vec3>()(vertex.position) ^
				hash<glm::vec2>()(vertex.uv) << 1) >> 1 ^
			hash<glm::vec3>()(vertex.normal) << 1;
	}
};
