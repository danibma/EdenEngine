#pragma once

#include "Graphics/D3D12/D3D12RHI.h"

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
				std::shared_ptr<Texture2D> diffuse_texture;
				std::shared_ptr<Texture2D> emissive_texture;
				uint32_t vertex_start;
				uint32_t index_start;
				uint32_t index_count;
			};

			std::vector<SubMesh> submeshes;
			glm::mat4 gltf_matrix = glm::mat4(1.0f);
		};

		uint32_t vertex_count;
		uint32_t index_count;
		std::shared_ptr<Buffer> mesh_vb;
		std::shared_ptr<Buffer> mesh_ib;
		std::shared_ptr<Buffer> transform_cb;
		std::vector<Mesh> meshes;
		std::vector<std::shared_ptr<Texture2D>> textures;
		bool has_mesh = false;

		MeshSource() = default;
		void LoadGLTF(std::shared_ptr<D3D12RHI>& gfx, std::filesystem::path file);
		void Destroy();

		~MeshSource()
		{
			Destroy();
		}

	private:
		std::shared_ptr<Texture2D> LoadImage(std::shared_ptr<D3D12RHI>& gfx, tinygltf::Model& gltf_model, int32_t image_index);
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
