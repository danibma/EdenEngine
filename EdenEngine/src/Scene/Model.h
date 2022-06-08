#pragma once

#include "Graphics/D3D12RHI.h"

#include <functional>
#include <vector>
#include <filesystem>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <tinygltf/tiny_gltf.h>

namespace Eden
{
	//==================
	// kVertex
	//==================
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

	//==================
	// Model
	//==================
	struct Model
	{
		struct Mesh
		{
			struct SubMesh
			{
				uint32_t material_index;
				uint32_t vertex_start;
				uint32_t index_start;
				uint32_t index_count;
			};

			std::vector<SubMesh> submeshes;

			// Transform
			// NOTE(Daniel): I don't think this needs to be inside Mesh, I think it can be inside Model, but currently I'm not going to worry about this
			glm::mat4 transform = glm::mat4(1.0f);
			glm::mat4 gltf_matrix = glm::mat4(1.0f); // Do not use this for outside of this struct
			Buffer transform_cb;

			void UpdateTransform();
			void SetTranslation(int32_t x, int32_t y, int32_t z);
			void SetRotation(float angle, int32_t x, int32_t y, int32_t z);
			void SetScale(int32_t x, int32_t y, int32_t z);

		private:
			glm::mat4 m_Scale = glm::mat4(1.0f);
			glm::mat4 m_Rotation = glm::mat4(1.0f);
			glm::mat4 m_Translation = glm::mat4(1.0f);

			glm::vec3 m_LastScale = glm::vec3(1.0f);
			glm::vec3 m_LastRotation = glm::vec3(0.0f);
			float m_LastAngle = 0.0f;
			glm::vec3 m_LastTranslation = glm::vec3(0.0f);
		};

		std::vector<VertexData> vertices;
		Buffer mesh_vb;
		std::vector<uint32_t> indices;
		Buffer mesh_ib;

		std::vector<Mesh> meshes;

		std::vector<Texture2D> textures;

		void LoadGLTF(D3D12RHI* gfx, std::filesystem::path file);
		void Destroy();

	private:
		uint32_t LoadImage(D3D12RHI* gfx, tinygltf::Model& gltf_model, int32_t image_index);
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
