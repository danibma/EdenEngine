#pragma once

#include "Graphics/GraphicsDevice.h"

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
	// Vertex
	//==================
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		glm::vec3 normal;
		glm::vec4 color;

		bool operator==(const Vertex& other) const
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
				uint32_t materialIndex;
				uint32_t vertexStart;
				uint32_t indexStart;
				uint32_t indexCount;
			};

			std::vector<SubMesh> submeshes;

			// Transform
			// NOTE(Daniel): I don't think this needs to be inside Mesh, I think it can be inside Model, but currently I'm not going to worry about this
			glm::mat4 transform = glm::mat4(1.0f);
			glm::mat4 gltfMatrix = glm::mat4(1.0f); // Do not use this for outside of this struct
			Buffer transformCB;

			void UpdateTransform();
			void SetTranslation(int32_t x, int32_t y, int32_t z);
			void SetRotation(float angle, int32_t x, int32_t y, int32_t z);
			void SetScale(int32_t x, int32_t y, int32_t z);

		private:
			glm::mat4 m_scale = glm::mat4(1.0f);
			glm::mat4 m_rotation = glm::mat4(1.0f);
			glm::mat4 m_translation = glm::mat4(1.0f);

			glm::vec3 m_lastScale = glm::vec3(1.0f);
			glm::vec3 m_lastRotation = glm::vec3(0.0f);
			float m_lastAngle = 0.0f;
			glm::vec3 m_lastTranslation = glm::vec3(0.0f);
		};

		std::vector<Vertex> vertices;
		Buffer meshVB;
		std::vector<uint32_t> indices;
		Buffer meshIB;

		std::vector<Mesh> meshes;

		std::vector<Texture2D> textures;

		void LoadGLTF(GraphicsDevice* gfx, std::filesystem::path file);
		void Destroy();

	private:
		uint32_t LoadImage(GraphicsDevice* gfx, tinygltf::Model& gltfModel, int32_t imageIndex);
	};
}

namespace std
{
	template<> struct hash<Eden::Vertex> {
		size_t operator()(Eden::Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.position) ^
					 (hash<glm::vec2>()(vertex.uv) << 1)) >> 1) ^
				(hash<glm::vec3>()(vertex.normal) << 1);
		}
	};
}
