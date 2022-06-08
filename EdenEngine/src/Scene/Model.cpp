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
	void Model::LoadGLTF(D3D12RHI* gfx, std::filesystem::path file)
	{
		tinygltf::Model gltf_model;
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;
		bool result = false;

		if (file.extension() == ".gltf")
			result = loader.LoadASCIIFromFile(&gltf_model, &err, &warn, file.string());
		else if (file.extension() == ".glb")
			result = loader.LoadBinaryFromFile(&gltf_model, &err, &warn, file.string());

		ED_LOG_INFO("Starting {} file loading", file);

		if (!warn.empty())
			ED_LOG_WARN("{}", warn.c_str());

		if (!err.empty())
			ED_LOG_ERROR("{}", err.c_str());

		ED_ASSERT_LOG(result, "Failed to parse GLTF Model!");

		std::string parent_path = file.parent_path().string() + "/";

		for (const auto& gltf_node : gltf_model.nodes)
		{
			if (gltf_node.mesh < 0)
				continue;

			Mesh mesh;

			// Get the local node matrix
			// It's either made up from translation, rotation, scale or a 4x4 matrix
			if (gltf_node.translation.size() == 3)
			{
				mesh.gltf_matrix = glm::translate(mesh.gltf_matrix, glm::vec3(glm::make_vec3(gltf_node.translation.data())));
			}

			if (gltf_node.rotation.size() == 4)
			{
				glm::quat q = glm::make_quat(gltf_node.rotation.data());
				auto rotation = glm::quat(q.w, -q.x, q.y, q.z);
				mesh.gltf_matrix *= glm::mat4(rotation);
			}

			if (gltf_node.scale.size() == 3)
			{
				mesh.gltf_matrix = glm::scale(mesh.gltf_matrix, glm::vec3(glm::make_vec3(gltf_node.scale.data())));
			}

			if (gltf_node.matrix.size() == 16)
			{
				mesh.gltf_matrix = glm::make_mat4x4(gltf_node.matrix.data());
			}

			mesh.transform *= mesh.gltf_matrix;
			mesh.transform_cb = gfx->CreateBuffer<glm::mat4>(&mesh.transform, 1);

			const auto& gltf_mesh = gltf_model.meshes[gltf_node.mesh];
			for (size_t p = 0; p < gltf_mesh.primitives.size(); ++p)
			{
				Mesh::SubMesh submesh;
				auto& gltf_primitive = gltf_mesh.primitives[p];
				submesh.vertex_start = (uint32_t)vertices.size();
				submesh.index_start = (uint32_t)indices.size();
				submesh.index_count = 0;

				// Vertices
				{
					const float* position_buffer = nullptr;
					const float* normals_buffer = nullptr;
					const float* uvs_buffer = nullptr;
					size_t vertex_count = 0;

					// Get buffer data for vertex normals
					if (gltf_primitive.attributes.find("POSITION") != gltf_primitive.attributes.end())
					{
						const tinygltf::Accessor& accessor = gltf_model.accessors[gltf_primitive.attributes.find("POSITION")->second];
						const tinygltf::BufferView& view = gltf_model.bufferViews[accessor.bufferView];
						position_buffer = reinterpret_cast<const float*>(&(gltf_model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
						vertex_count = accessor.count;
					}

					// Get buffer data for vertex normals
					if (gltf_primitive.attributes.find("NORMAL") != gltf_primitive.attributes.end())
					{
						const tinygltf::Accessor& accessor = gltf_model.accessors[gltf_primitive.attributes.find("NORMAL")->second];
						const tinygltf::BufferView& view = gltf_model.bufferViews[accessor.bufferView];
						normals_buffer = reinterpret_cast<const float*>(&(gltf_model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					}

					// Get buffer data for vertex texture coordinates
					// glTF supports multiple sets, we only load the first one
					if (gltf_primitive.attributes.find("TEXCOORD_0") != gltf_primitive.attributes.end())
					{
						const tinygltf::Accessor& accessor = gltf_model.accessors[gltf_primitive.attributes.find("TEXCOORD_0")->second];
						const tinygltf::BufferView& view = gltf_model.bufferViews[accessor.bufferView];
						uvs_buffer = reinterpret_cast<const float*>(&(gltf_model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					}

					for (size_t v = 0; v < vertex_count; ++v)
					{
						VertexData new_vert = {};

						new_vert.position = glm::make_vec3(&position_buffer[v * 3]);
						//newVert.position.y = glm::make_vec3(&positionBuffer[v * 3]).z;
						//newVert.position.z = glm::make_vec3(&positionBuffer[v * 3]).y;
						new_vert.position.z = -new_vert.position.z;

						new_vert.normal = glm::normalize(glm::vec3(normals_buffer ? glm::make_vec3(&normals_buffer[v * 3]) : glm::vec3(0.0f)));
						//newVert.normal.y = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f))).z;
						//newVert.normal.z = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f))).y;
						new_vert.normal.z = -new_vert.normal.z;

						new_vert.uv = uvs_buffer ? glm::make_vec2(&uvs_buffer[v * 2]) : glm::vec2(0.0f);

						if (gltf_model.materials[gltf_primitive.material].values.find("baseColorFactor") != gltf_model.materials[gltf_primitive.material].values.end())
							new_vert.color = glm::make_vec4(gltf_model.materials[gltf_primitive.material].values["baseColorFactor"].ColorFactor().data());
						else
							new_vert.color = glm::vec4(1.0f);

						vertices.push_back(new_vert);
					}
				}

				// Indices
				{
					const tinygltf::Accessor& accessor = gltf_model.accessors[gltf_primitive.indices];
					const tinygltf::BufferView& buffer_view = gltf_model.bufferViews[accessor.bufferView];
					const tinygltf::Buffer& buffer = gltf_model.buffers[buffer_view.buffer];

					submesh.index_count += static_cast<uint32_t>(accessor.count);

					// glTF supports different component types of indices
					switch (accessor.componentType)
					{
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
					{
						uint32_t* buf = new uint32_t[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + buffer_view.byteOffset], accessor.count * sizeof(uint32_t));
						for (size_t index = 0; index < accessor.count; index++)
							indices.push_back(buf[index] + submesh.vertex_start);

						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
					{
						uint16_t* buf = new uint16_t[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + buffer_view.byteOffset], accessor.count * sizeof(uint16_t));
						for (size_t index = 0; index < accessor.count; index++)
							indices.push_back(buf[index] + submesh.vertex_start);

						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
					{
						uint8_t* buf = new uint8_t[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + buffer_view.byteOffset], accessor.count * sizeof(uint8_t));
						for (size_t index = 0; index < accessor.count; index++)
							indices.push_back(buf[index] + submesh.vertex_start);

						break;
					}
					default:
						ED_LOG_ERROR("Index component type {} not supported!");
					}
				}

				// Load materials
				auto gltf_material = gltf_model.materials[gltf_primitive.material];
				if (gltf_material.values.find("baseColorTexture") != gltf_material.values.end())
				{
					auto material_index = gltf_material.values["baseColorTexture"].TextureIndex();
					auto texture_index = gltf_model.textures[material_index].source;

					// This assumes every primitive that has a texture, has a diffuse
					// so the diffuse is always the first texture of the "Material"
					submesh.material_index = LoadImage(gfx, gltf_model, texture_index);
				}

				if (gltf_material.emissiveTexture.index > -1)
				{
					auto material_index = gltf_material.emissiveTexture.index;
					auto texture_index = gltf_model.textures[material_index].source;

					LoadImage(gfx, gltf_model, texture_index);
				}

				mesh.submeshes.push_back(submesh);
			}

			meshes.push_back(mesh);
		}

		mesh_vb = gfx->CreateBuffer<VertexData>(vertices.data(), (uint32_t)vertices.size());
		mesh_ib = gfx->CreateBuffer<uint32_t>(indices.data(), (uint32_t)indices.size());

		ED_LOG_INFO("	{} nodes were loaded!", gltf_model.nodes.size());
		ED_LOG_INFO("	{} meshes were loaded!", gltf_model.meshes.size());
		ED_LOG_INFO("	{} textures were loaded!", gltf_model.textures.size());
		ED_LOG_INFO("	{} materials were loaded!", gltf_model.materials.size());
	}

	void Model::Destroy()
	{
		mesh_vb.Release();
		mesh_ib.Release();

		for (auto& texture : textures)
			texture.Release();

		textures.clear();

		for (auto& mesh : meshes)
			mesh.transform_cb.Release();
	}

	uint32_t Model::LoadImage(D3D12RHI* gfx, tinygltf::Model& gltf_model, int32_t image_index)
	{
		tinygltf::Image& gltf_image = gltf_model.images[image_index];

		unsigned char* buffer;
		bool delete_buffer = false;

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

			delete_buffer = true;
		}
		else
		{
			buffer = &gltf_image.image[0];
		}

		auto texture_id = textures.emplace_back(gfx->CreateTexture2D(buffer, gltf_image.width, gltf_image.height)).heap_offset;

		if (delete_buffer)
			edelete buffer;

		return texture_id;
	}

	// Transform
	void Model::Mesh::UpdateTransform()
	{
		transform = m_Scale * m_Rotation * m_Translation * gltf_matrix;
	}

	void Model::Mesh::SetTranslation(int32_t x, int32_t y, int32_t z)
	{
		glm::vec3 new_translation = glm::vec3(x, y, z);
		if (m_LastTranslation != new_translation)
		{
			m_Translation = glm::translate(m_Translation, new_translation);
			m_LastTranslation = new_translation;
		}
	}

	void Model::Mesh::SetRotation(float angle, int32_t x, int32_t y, int32_t z)
	{
		glm::vec3 new_rotation = glm::vec3(x, y, z);
		if (new_rotation != m_LastRotation && angle != m_LastAngle)
		{
			m_Rotation = glm::rotate(m_Rotation, glm::radians(angle), new_rotation);
			m_LastRotation = new_rotation;
		}
	}

	void Model::Mesh::SetScale(int32_t x, int32_t y, int32_t z)
	{
		glm::vec3 new_scale = glm::vec3(x, y, z);
		if (new_scale != m_LastScale)
		{
			m_Scale = glm::scale(m_Scale, glm::vec3(x, y, z));;
			m_LastScale = new_scale;
		}
	}

}
