#include "Model.h"

#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#define TINYGLTF_USE_CPP14
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>

#include "Core/Memory.h"

namespace Eden
{
	// Based on Sascha Willems gltfloading.cpp
	void Model::LoadGLTF(GraphicsDevice* gfx, std::filesystem::path file)
	{
		tinygltf::Model gltfModel;
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;
		bool result = false;

		if (file.extension() == ".gltf")
			result = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, file.string());
		else if (file.extension() == ".glb")
			result = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, file.string());

		ED_LOG_INFO("Starting {} file loading", file);

		if (!warn.empty())
			ED_LOG_WARN("{}", warn.c_str());

		if (!err.empty())
			ED_LOG_ERROR("{}", err.c_str());

		ED_ASSERT_LOG(result, "Failed to parse GLTF Model!");

		std::string parentPath = file.parent_path().string() + "/";

		for (const auto& gltf_node : gltfModel.nodes)
		{
			if (gltf_node.mesh < 0)
				continue;

			Mesh mesh;

			// Get the local node matrix
			// It's either made up from translation, rotation, scale or a 4x4 matrix
			if (gltf_node.translation.size() == 3)
			{
				mesh.gltfMatrix = glm::translate(mesh.gltfMatrix, glm::vec3(glm::make_vec3(gltf_node.translation.data())));
			}

			if (gltf_node.rotation.size() == 4)
			{
				glm::quat q = glm::make_quat(gltf_node.rotation.data());
				auto rotation = glm::quat(q.w, -q.x, q.y, q.z);
				mesh.gltfMatrix *= glm::mat4(rotation);
			}

			if (gltf_node.scale.size() == 3)
			{
				mesh.gltfMatrix = glm::scale(mesh.gltfMatrix, glm::vec3(glm::make_vec3(gltf_node.scale.data())));
			}

			if (gltf_node.matrix.size() == 16)
			{
				mesh.gltfMatrix = glm::make_mat4x4(gltf_node.matrix.data());
			}

			mesh.transform *= mesh.gltfMatrix;
			mesh.transformCB = gfx->CreateBuffer<glm::mat4>(&mesh.transform, 1);

			const auto& gltf_mesh = gltfModel.meshes[gltf_node.mesh];
			for (size_t p = 0; p < gltf_mesh.primitives.size(); ++p)
			{
				Mesh::SubMesh submesh;
				auto& gltf_primitive = gltf_mesh.primitives[p];
				submesh.vertexStart = (uint32_t)vertices.size();
				submesh.indexStart = (uint32_t)indices.size();
				submesh.indexCount = 0;

				// Vertices
				{
					const float* positionBuffer = nullptr;
					const float* normalsBuffer = nullptr;
					const float* uvsBuffer = nullptr;
					size_t vertexCount = 0;

					// Get buffer data for vertex normals
					if (gltf_primitive.attributes.find("POSITION") != gltf_primitive.attributes.end())
					{
						const tinygltf::Accessor& accessor = gltfModel.accessors[gltf_primitive.attributes.find("POSITION")->second];
						const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
						positionBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
						vertexCount = accessor.count;
					}

					// Get buffer data for vertex normals
					if (gltf_primitive.attributes.find("NORMAL") != gltf_primitive.attributes.end())
					{
						const tinygltf::Accessor& accessor = gltfModel.accessors[gltf_primitive.attributes.find("NORMAL")->second];
						const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
						normalsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					}

					// Get buffer data for vertex texture coordinates
					// glTF supports multiple sets, we only load the first one
					if (gltf_primitive.attributes.find("TEXCOORD_0") != gltf_primitive.attributes.end())
					{
						const tinygltf::Accessor& accessor = gltfModel.accessors[gltf_primitive.attributes.find("TEXCOORD_0")->second];
						const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
						uvsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					}

					for (size_t v = 0; v < vertexCount; ++v)
					{
						VertexData newVert = {};

						newVert.position = glm::make_vec3(&positionBuffer[v * 3]);
						//newVert.position.y = glm::make_vec3(&positionBuffer[v * 3]).z;
						//newVert.position.z = glm::make_vec3(&positionBuffer[v * 3]).y;
						newVert.position.z = -newVert.position.z;

						newVert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
						//newVert.normal.y = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f))).z;
						//newVert.normal.z = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f))).y;
						newVert.normal.z = -newVert.normal.z;

						newVert.uv = uvsBuffer ? glm::make_vec2(&uvsBuffer[v * 2]) : glm::vec2(0.0f);

						if (gltfModel.materials[gltf_primitive.material].values.find("baseColorFactor") != gltfModel.materials[gltf_primitive.material].values.end())
							newVert.color = glm::make_vec4(gltfModel.materials[gltf_primitive.material].values["baseColorFactor"].ColorFactor().data());
						else
							newVert.color = glm::vec4(1.0f);

						vertices.push_back(newVert);
					}
				}

				// Indices
				{
					const tinygltf::Accessor& accessor = gltfModel.accessors[gltf_primitive.indices];
					const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
					const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];

					submesh.indexCount += static_cast<uint32_t>(accessor.count);

					// glTF supports different component types of indices
					switch (accessor.componentType)
					{
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
					{
						uint32_t* buf = new uint32_t[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint32_t));
						for (size_t index = 0; index < accessor.count; index++)
							indices.push_back(buf[index] + submesh.vertexStart);

						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
					{
						uint16_t* buf = new uint16_t[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
						for (size_t index = 0; index < accessor.count; index++)
							indices.push_back(buf[index] + submesh.vertexStart);

						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
					{
						uint8_t* buf = new uint8_t[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
						for (size_t index = 0; index < accessor.count; index++)
							indices.push_back(buf[index] + submesh.vertexStart);

						break;
					}
					default:
						ED_LOG_ERROR("Index component type {} not supported!");
					}
				}

				// Load materials
				auto gltf_material = gltfModel.materials[gltf_primitive.material];
				if (gltf_material.values.find("baseColorTexture") != gltf_material.values.end())
				{
					auto materialIndex = gltf_material.values["baseColorTexture"].TextureIndex();
					auto textureIndex = gltfModel.textures[materialIndex].source;

					// This assumes every primitive that has a texture, has a diffuse
					// so the diffuse is always the first texture of the "Material"
					submesh.materialIndex = LoadImage(gfx, gltfModel, textureIndex);
				}

				if (gltf_material.emissiveTexture.index > -1)
				{
					auto materialIndex = gltf_material.emissiveTexture.index;
					auto textureIndex = gltfModel.textures[materialIndex].source;

					LoadImage(gfx, gltfModel, textureIndex);
				}

				mesh.submeshes.push_back(submesh);
			}

			meshes.push_back(mesh);
		}

		meshVB = gfx->CreateBuffer<VertexData>(vertices.data(), (uint32_t)vertices.size());
		meshIB = gfx->CreateBuffer<uint32_t>(indices.data(), (uint32_t)indices.size());

		ED_LOG_INFO("	{} nodes were loaded!", gltfModel.nodes.size());
		ED_LOG_INFO("	{} meshes were loaded!", gltfModel.meshes.size());
		ED_LOG_INFO("	{} textures were loaded!", gltfModel.textures.size());
		ED_LOG_INFO("	{} materials were loaded!", gltfModel.materials.size());
	}

	void Model::Destroy()
	{
		meshVB.Release();
		meshIB.Release();

		for (auto& texture : textures)
			texture.Release();

		textures.clear();

		for (auto& mesh : meshes)
			mesh.transformCB.Release();
	}

	uint32_t Model::LoadImage(GraphicsDevice* gfx, tinygltf::Model& gltfModel, int32_t imageIndex)
	{
		tinygltf::Image& gltf_image = gltfModel.images[imageIndex];

		unsigned char* buffer = nullptr;
		bool deleteBuffer = false;

		if (gltf_image.component == 3)
		{
			uint64_t bufferSize = gltf_image.width * gltf_image.height * 4;
			buffer = enew unsigned char[bufferSize];
			unsigned char* rgba = buffer;
			unsigned char* rgb = &gltf_image.image[0];

			for (size_t i = 0; i < gltf_image.width * gltf_image.height; ++i)
			{
				memcpy(rgba, rgb, sizeof(unsigned char) * 3);
				rgba += 4;
				rgb += 3;
			}

			deleteBuffer = true;
		}
		else
		{
			buffer = &gltf_image.image[0];
		}

		auto textureID = textures.emplace_back(gfx->CreateTexture2D(buffer, gltf_image.width, gltf_image.height)).heapOffset;

		if (deleteBuffer)
			edelete buffer;

		return textureID;
	}

	// Transform
	void Model::Mesh::UpdateTransform()
	{
		transform = m_scale * m_rotation * m_translation * gltfMatrix;
	}

	void Model::Mesh::SetTranslation(int32_t x, int32_t y, int32_t z)
	{
		glm::vec3 newTranslation = glm::vec3(x, y, z);
		if (m_lastTranslation != newTranslation)
		{
			m_translation = glm::translate(m_translation, newTranslation);
			m_lastTranslation = newTranslation;
		}
	}

	void Model::Mesh::SetRotation(float angle, int32_t x, int32_t y, int32_t z)
	{
		glm::vec3 newRotation = glm::vec3(x, y, z);
		if (newRotation != m_lastRotation && angle != m_lastAngle)
		{
			m_rotation = glm::rotate(m_rotation, glm::radians(angle), newRotation);
			m_lastRotation = newRotation;
		}
	}

	void Model::Mesh::SetScale(int32_t x, int32_t y, int32_t z)
	{
		glm::vec3 newScale = glm::vec3(x, y, z);
		if (newScale != m_lastScale)
		{
			m_scale = glm::scale(m_scale, glm::vec3(x, y, z));;
			m_lastScale = newScale;
		}
	}

}
