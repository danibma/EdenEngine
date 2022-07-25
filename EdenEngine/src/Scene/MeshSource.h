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
				std::shared_ptr<Texture> diffuse_texture;
				std::shared_ptr<Texture> emissive_texture;
				uint32_t vertex_start;
				uint32_t index_start;
				uint32_t index_count;
			};

			std::vector<std::shared_ptr<SubMesh>> submeshes;
			glm::mat4 gltf_matrix = glm::mat4(1.0f);
		};

		uint32_t vertex_count;
		uint32_t index_count;
		std::shared_ptr<Buffer> mesh_vb;
		std::shared_ptr<Buffer> mesh_ib;
		std::vector<std::shared_ptr<Mesh>> meshes;
		bool has_mesh = false;
		bool textured = false;

		MeshSource() = default;
		void LoadGLTF(std::shared_ptr<IRHI>& rhi, std::filesystem::path file);
		void Destroy();

		~MeshSource()
		{
			Destroy();
		}

	private:
		std::shared_ptr<Texture> LoadImage(std::shared_ptr<IRHI>& rhi, tinygltf::Model& gltf_model, int32_t image_index);
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
