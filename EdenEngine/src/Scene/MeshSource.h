#pragma once

#include "RHI/RHI.h"

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

	struct PBRMaterial
	{
		Texture albedoMap;
		Texture normalMap;
		Texture AOMap;
		Texture emissiveMap;
		Texture metallicRoughnessMap; // r = metallic, g = roughness
	};

	struct MeshSource
	{
		struct Mesh
		{
			struct SubMesh
			{
				PBRMaterial material;
				uint32_t vertexStart;
				uint32_t indexStart;
				uint32_t indexCount;
			};

			std::vector<SharedPtr<SubMesh>> submeshes;
			glm::mat4 modelMatrix = glm::mat4(1.0f);
		};

		uint32_t vertexCount;
		uint32_t indexCount;
		Buffer meshVb;
		Buffer meshIb;
		std::vector<SharedPtr<Mesh>> meshes;
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
		Texture m_BlackTexture;

	private:
		void LoadMaterial(tinygltf::Model& gltfModel, const tinygltf::Primitive& gltfPrimitive, PBRMaterial& material);
		void LoadImage(Texture* texture, tinygltf::Model& gltfModel, int32_t imageIndex);
		void LoadNode(tinygltf::Model& gltfModel, const tinygltf::Node& gltfNode, const glm::mat4* parentMatrix, std::vector<VertexData>& vertices, std::vector<uint32_t>& indices);
		void LoadMesh(tinygltf::Model& gltfModel, const tinygltf::Node& gltfNode, glm::mat4& modelMatrix, std::vector<VertexData>& vertices, std::vector<uint32_t>& indices);
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
