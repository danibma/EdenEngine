#include "MeshSource.h"

#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#include <stb/stb_image_write.h>

#define TINYGLTF_USE_CPP14
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>

#include "Core/Memory/Memory.h"
#include "Core/Log.h"
#include "Core/Base.h"
#include "Core/Assertions.h"
#include "Graphics/RHI.h"
#include "Graphics/Renderer.h"

namespace Eden
{
	// Based on Sascha Willems gltfloading.cpp
	void MeshSource::LoadGLTF(std::filesystem::path file)
	{
		// Destroy the current mesh source
		if (bHasMesh)
			Destroy();

		BufferDesc desc;
		desc.usage = BufferDesc::Uniform;
		desc.elementCount = 1;
		desc.stride = sizeof(glm::mat4);

		tinygltf::Model gltfModel;
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;
		bool bIsGLTFModelValid = false;

		if (file.extension() == ".gltf")
			bIsGLTFModelValid = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, file.string());
		else if (file.extension() == ".glb")
			bIsGLTFModelValid = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, file.string());

		ED_LOG_INFO("Starting {} file loading", file);

		if (!warn.empty())
			ED_LOG_WARN("{}", warn.c_str());

		if (!err.empty())
			ED_LOG_ERROR("{}", err.c_str());

		ensureMsg(bIsGLTFModelValid, "Failed to parse GLTF Model!");


		uint32_t blackTextureData = 0xff000000;
		TextureDesc blackDesc = {};
		blackDesc.data = &blackTextureData;
		blackDesc.width = 1;
		blackDesc.height = 1;
		blackDesc.bIsStorage = false;
		blackDesc.bGenerateMips = false;

		const tinygltf::Scene& scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];
		Renderer::CreateTexture(&m_BlackTexture, &blackDesc);

		std::vector<VertexData> vertices;
		std::vector<uint32_t> indices;
		for (size_t nodeIndex = 0; nodeIndex < scene.nodes.size(); ++nodeIndex)
		{
			const tinygltf::Node gltfNode = gltfModel.nodes[scene.nodes[nodeIndex]];
			LoadNode(gltfModel, gltfNode, nullptr, vertices, indices);
		}

		vertexCount = static_cast<uint32_t>(vertices.size());
		indexCount = static_cast<uint32_t>(indices.size());

		BufferDesc vbDesc;
		vbDesc.elementCount = vertexCount;
		vbDesc.stride = sizeof(VertexData);
		vbDesc.usage = BufferDesc::Vertex_Index;
		Renderer::CreateBuffer(&meshVb, &vbDesc, vertices.data());

		BufferDesc ibDesc;
		ibDesc.elementCount = indexCount;
		ibDesc.stride = sizeof(uint32_t);
		ibDesc.usage = BufferDesc::Vertex_Index;
		Renderer::CreateBuffer(&meshIb, &ibDesc, indices.data());
		bHasMesh = true;

		ED_LOG_INFO("	{} nodes were loaded!", gltfModel.nodes.size());
		ED_LOG_INFO("	{} meshes were loaded!", gltfModel.meshes.size());
		ED_LOG_INFO("	{} textures were loaded!", gltfModel.textures.size());
		ED_LOG_INFO("	{} materials were loaded!", gltfModel.materials.size());
		ED_LOG_INFO("   {} vertices were loaded!", vertexCount);
	}

	void MeshSource::Destroy()
	{
		meshes.clear();
	}
	
	void MeshSource::LoadMesh(tinygltf::Model& gltfModel, const tinygltf::Node& gltfNode, glm::mat4& modelMatrix, std::vector<VertexData>& vertices, std::vector<uint32_t>& indices)
	{
		SharedPtr<Mesh> mesh = MakeShared<Mesh>();
		mesh->modelMatrix = modelMatrix;
	
		const auto& gltfMesh = gltfModel.meshes[gltfNode.mesh];
		for (size_t p = 0; p < gltfMesh.primitives.size(); ++p)
		{
			SharedPtr<Mesh::SubMesh> submesh = MakeShared<Mesh::SubMesh>();
			auto& gltfPrimitive = gltfMesh.primitives[p];
			submesh->vertexStart = (uint32_t)vertices.size();
			submesh->indexStart = (uint32_t)indices.size();
			submesh->indexCount = 0;
	
			// Vertices
			{
				const float* positionBuffer = nullptr;
				const float* normalsBuffer = nullptr;
				const float* uvsBuffer = nullptr;
				size_t vertexCount = 0;
	
				// Get buffer data for vertex normals
				if (gltfPrimitive.attributes.find("POSITION") != gltfPrimitive.attributes.end())
				{
					const tinygltf::Accessor& accessor = gltfModel.accessors[gltfPrimitive.attributes.find("POSITION")->second];
					const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
					positionBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					vertexCount = accessor.count;
				}
	
				// Get buffer data for vertex normals
				if (gltfPrimitive.attributes.find("NORMAL") != gltfPrimitive.attributes.end())
				{
					const tinygltf::Accessor& accessor = gltfModel.accessors[gltfPrimitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
					normalsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
	
				// Get buffer data for vertex texture coordinates
				// glTF supports multiple sets, we only load the first one
				if (gltfPrimitive.attributes.find("TEXCOORD_0") != gltfPrimitive.attributes.end())
				{
					const tinygltf::Accessor& accessor = gltfModel.accessors[gltfPrimitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
					uvsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
	
				for (size_t v = 0; v < vertexCount; ++v)
				{
					VertexData newVert = {};
	
					newVert.position = glm::make_vec3(&positionBuffer[v * 3]);
					newVert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
					newVert.uv = uvsBuffer ? glm::make_vec2(&uvsBuffer[v * 2]) : glm::vec2(0.0f);
	
					if (gltfModel.materials[gltfPrimitive.material].values.find("baseColorFactor") != gltfModel.materials[gltfPrimitive.material].values.end())
						newVert.color = glm::make_vec4(gltfModel.materials[gltfPrimitive.material].values["baseColorFactor"].ColorFactor().data());
					else
						newVert.color = glm::vec4(1.0f);
	
					vertices.emplace_back(newVert);
				}
			}
	
			// Indices
			{
				const tinygltf::Accessor& accessor = gltfModel.accessors[gltfPrimitive.indices];
				const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];
	
				submesh->indexCount += static_cast<uint32_t>(accessor.count);
	
				// glTF supports different component types of indices
				switch (accessor.componentType)
				{
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
					{
						uint32_t* buf = enew uint32_t[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint32_t));
						for (size_t index = 0; index < accessor.count; index++)
							indices.emplace_back(buf[index] + submesh->vertexStart);
						edelete[] buf;
						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
					{
						uint16_t* buf = enew uint16_t[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
						for (size_t index = 0; index < accessor.count; index++)
							indices.emplace_back(buf[index] + submesh->vertexStart);
						edelete[] buf;
						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
					{
						uint8_t* buf = enew uint8_t[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
						for (size_t index = 0; index < accessor.count; index++)
							indices.emplace_back(buf[index] + submesh->vertexStart);
						edelete[] buf;
						break;
					}
					default:
						ED_LOG_ERROR("Index component type {} not supported!");
				}
			}
	
			// Load materials
			LoadMaterial(gltfModel, gltfPrimitive, submesh->material);
	
			mesh->submeshes.emplace_back(submesh);
		}
	
		meshes.emplace_back(mesh);
	}

	void MeshSource::LoadMaterial(tinygltf::Model& gltfModel, const tinygltf::Primitive& gltfPrimitive, PBRMaterial& material)
	{
		auto& gltfMaterial = gltfModel.materials[gltfPrimitive.material];
		// Albedo
		if (gltfMaterial.values.find("baseColorTexture") != gltfMaterial.values.end())
		{
			auto material_index = gltfMaterial.values["baseColorTexture"].TextureIndex();
			auto textureIndex = gltfModel.textures[material_index].source;
			
			LoadImage(&material.albedoMap, gltfModel, textureIndex);
		}
		else
		{
			material.albedoMap = m_BlackTexture;
		}

		// Metallic = r, Roughness = g
		if (gltfMaterial.values.find("metallicRoughnessTexture") != gltfMaterial.values.end())
		{
			auto material_index = gltfMaterial.values["metallicRoughnessTexture"].TextureIndex();
			auto textureIndex = gltfModel.textures[material_index].source;

			LoadImage(&material.metallicRoughnessMap, gltfModel, textureIndex);
		}
		else
		{
			material.metallicRoughnessMap = m_BlackTexture;
		}

		// Normal
		if (gltfMaterial.normalTexture.index > -1)
		{
			auto material_index = gltfMaterial.normalTexture.index;
			auto textureIndex = gltfModel.textures[material_index].source;
			
			LoadImage(&material.normalMap, gltfModel, textureIndex);
		}
		else
		{
			material.normalMap = m_BlackTexture;
		}

		// AO
		if (gltfMaterial.occlusionTexture.index > -1)
		{
			auto material_index = gltfMaterial.occlusionTexture.index;
			auto textureIndex = gltfModel.textures[material_index].source;
			
			LoadImage(&material.AOMap, gltfModel, textureIndex);
		}
		else
		{
			material.AOMap = m_BlackTexture;
		}
	
		// Emissive
		if (gltfMaterial.emissiveTexture.index > -1)
		{
			auto material_index = gltfMaterial.emissiveTexture.index;
			auto textureIndex = gltfModel.textures[material_index].source;
			
			LoadImage(&material.emissiveMap, gltfModel, textureIndex);
		}
		else
		{
			material.emissiveMap = m_BlackTexture;
		}
	}

	void MeshSource::LoadNode(tinygltf::Model& gltfModel, const tinygltf::Node& gltfNode, const glm::mat4* parentMatrix, std::vector<VertexData>& vertices, std::vector<uint32_t>& indices)
	{
		glm::mat4 modelMatrix = glm::mat4(1.0f);

		if (parentMatrix)
			modelMatrix = *parentMatrix;

		// Get the local node matrix
		// It's either made up from translation, rotation, scale or a 4x4 matrix
		if (gltfNode.translation.size() == 3)
		{
			glm::vec3 translation = glm::make_vec3(gltfNode.translation.data());
			modelMatrix *= glm::translate(glm::mat4(1.0f), translation);
		}

		if (gltfNode.rotation.size() == 4)
		{
			glm::quat q = glm::make_quat(gltfNode.rotation.data());
			auto rotation = glm::quat(q.w, q.x, q.y, q.z);
			modelMatrix *= glm::mat4(rotation);
		}

		if (gltfNode.scale.size() == 3)
		{
			glm::vec3 scale = glm::vec3(glm::make_vec3(gltfNode.scale.data()));
			modelMatrix *= glm::scale(glm::mat4(1.0f), scale);
		}

		if (gltfNode.matrix.size() == 16)
		{
			modelMatrix *= glm::mat4(glm::make_mat4x4(gltfNode.matrix.data()));
		}

		if (gltfNode.mesh >= 0)
			LoadMesh(gltfModel, gltfNode, modelMatrix, vertices, indices);

		for (size_t childIndex = 0; childIndex < gltfNode.children.size(); ++childIndex)
			LoadNode(gltfModel, gltfModel.nodes[gltfNode.children[childIndex]], &modelMatrix, vertices, indices);
	}

	void MeshSource::LoadImage(Texture* texture, tinygltf::Model& gltfModel, int32_t imageIndex)
	{
		tinygltf::Image& gltfImage = gltfModel.images[imageIndex];

		unsigned char* buffer;
		bool bDeleteBuffer = false;

		if (gltfImage.component == 3)
		{
			uint64_t bufferSize = gltfImage.width * gltfImage.height * 4;
			buffer = enew unsigned char[bufferSize];
			unsigned char* rgba = buffer;
			unsigned char* rgb = &gltfImage.image[0];

			for (size_t i = 0; i < gltfImage.width * gltfImage.height; ++i)
			{
				memcpy(rgba, rgb, sizeof(unsigned char) * 3);
				rgba += 4;
				rgb += 3;
			}

			bDeleteBuffer = true;
		}
		else
		{
			buffer = &gltfImage.image[0];
		}

		bIsTextured = true;
		TextureDesc desc = {};
		desc.data = buffer;
		desc.width = gltfImage.width;
		desc.height = gltfImage.height;
		desc.bIsStorage = false;
		Renderer::CreateTexture(texture, &desc);
		Renderer::SetName(texture, gltfImage.name);

		if (bDeleteBuffer)
			edelete buffer;
	}
}
