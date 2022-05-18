#include "Core/Window.h"
#include "Core/Memory.h"
#include "Core/Input.h"
#include "Core/Log.h"
#include "Profiling/Timer.h"
#include "Profiling/Profiler.h"
#include "Graphics/GraphicsDevice.h"
#include "Core/Camera.h"
#include "Utilities/Utils.h"

#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui/imgui.h>

#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#define TINYGLTF_USE_CPP14
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>

using namespace Eden;

Window* window;
GraphicsDevice* gfx;

// Timer stuff
Timer timer;
float deltaTime;

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

namespace std
{
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.position) ^
					 (hash<glm::vec2>()(vertex.uv) << 1)) >> 1) ^
				(hash<glm::vec3>()(vertex.normal) << 1);
		}
	};
}

// scene data
struct SceneData
{
	glm::mat4 MVPMatrix;
	glm::mat4 modelViewMatrix;
	// This matrix is used to fix the problem of a uniform scale only changing the normal's magnitude and not it's direction
	glm::mat4 normalMatrix;
	glm::vec3 lightPosition;
};

glm::mat4 model;
glm::mat4 view;
glm::mat4 projection;
glm::vec3 lightPosition(0.0f, 7.0f, 0.0f);
SceneData sceneData;
Camera camera;

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

		glm::mat4 matrix = glm::mat4(1.0f);
		Buffer meshCB;
	};

	std::vector<Vertex> vertices;
	Buffer meshVB;
	std::vector<uint32_t> indices;
	Buffer meshIB;

	std::vector<Mesh> meshes;

	std::vector<uint32_t> textureIndices;
	std::vector<uint32_t> materials;
	std::vector<Texture2D> textures;

	void LoadGLTF(std::filesystem::path file)
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
				mesh.matrix = glm::translate(mesh.matrix, glm::vec3(glm::make_vec3(gltf_node.translation.data())));
			}

			if (gltf_node.rotation.size() == 4)
			{
				glm::quat q = glm::make_quat(gltf_node.rotation.data());
				auto rotation = glm::quat(q.w, -q.x, q.y, q.z);
				mesh.matrix *= glm::mat4(rotation);
			}

			if (gltf_node.scale.size() == 3)
			{
				mesh.matrix = glm::scale(mesh.matrix, glm::vec3(glm::make_vec3(gltf_node.scale.data())));
			}
			
			if (gltf_node.matrix.size() == 16)
			{
				mesh.matrix = glm::make_mat4x4(gltf_node.matrix.data());
			}

			mesh.meshCB = gfx->CreateBuffer<SceneData>(&sceneData, 1);

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
					const float* positionBuffer;
					const float* normalsBuffer;
					const float* uvsBuffer;
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
						Vertex newVert = {};

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

				submesh.materialIndex = gltf_primitive.material;

				mesh.submeshes.push_back(submesh);
			}

			meshes.push_back(mesh);
		}

		textureIndices.resize(gltfModel.textures.size());
		for (size_t i = 0; i < gltfModel.textures.size(); ++i)
			textureIndices[i] = gltfModel.textures[i].source;

		textures.resize(gltfModel.images.size());
		for (size_t i = 0; i < gltfModel.images.size(); ++i)
			textures[i] = gfx->CreateTexture2D(parentPath + gltfModel.images[i].uri);

		materials.resize(gltfModel.materials.size());
		for (size_t i = 0; i < gltfModel.materials.size(); ++i)
		{
			if (gltfModel.materials[i].values.find("baseColorTexture") != gltfModel.materials[i].values.end())
				materials[i] = gltfModel.materials[i].values["baseColorTexture"].TextureIndex();
			else
				materials[i] = gltfModel.materials[i].values["baseColorFactor"].TextureIndex();
		}
			
		meshVB = gfx->CreateBuffer<Vertex>(vertices.data(), (uint32_t)vertices.size());
		meshIB = gfx->CreateBuffer<uint32_t>(indices.data(), (uint32_t)indices.size());
	}

	void Destroy()
	{
		meshVB.Release();
		meshIB.Release();

		for (auto& texture : textures)
			texture.Release();

		textures.clear();

		for (auto& mesh : meshes)
			mesh.meshCB.Release();
	}
};

Pipeline meshPipeline;
Model sponza;
Model flightHelmet;

void Init()
{
	Log::Init();

#ifdef ED_DEBUG
	window = enew Window("Eden Engine[DEBUG]", 1600, 900);
#else
	window = enew Window("Eden Engine", 1600, 900);
#endif 

	gfx = enew GraphicsDevice(window);
	gfx->EnableImGui();

	meshPipeline = gfx->CreateGraphicsPipeline("basic");

	sponza.LoadGLTF("assets/DamagedHelmet/DamagedHelmet.gltf");

	camera = Camera(window->GetWidth(), window->GetHeight());

	view = glm::lookAtLH(camera.position, camera.position + camera.front, camera.up);
	projection = glm::perspectiveFovLH_ZO(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 200.0f);
	model = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
	sceneData.MVPMatrix = projection * view * model;
	sceneData.modelViewMatrix = view * model;
	sceneData.lightPosition = lightPosition;

	ED_LOG_INFO("Scene Data size: {}", sizeof(SceneData));
}

bool openDebugWindow = true;
void Update()
{
	ED_PROFILE_FUNCTION();

	window->UpdateEvents();

	// Update timers
	deltaTime = (float)timer.ElapsedSeconds();
	timer.Record();

	if (!window->IsMinimized())
	{
		// Debug ImGui Window stuff
		if (Input::GetKeyDown(KeyCode::F3))
			openDebugWindow = !openDebugWindow;

		if (openDebugWindow)
		{
			ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_NoCollapse);
			ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize("Press F3 to close this window!").x) * 0.5f); // center text
			ImGui::TextDisabled("Press F3 to close this window!");
			if (ImGui::CollapsingHeader("Timers", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("CPU frame time: %.3fms", deltaTime * 1000.0f);
			}
			if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("Mesh Vertices: %d", sponza.vertices.size());
				ImGui::Text("Mesh Indices: %d", sponza.indices.size());
				ImGui::DragFloat3("Light Position", (float*)&lightPosition, 1.0f, 0.0f, 0.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::Text("Camera Position: %f, %f, %f", camera.position.x, camera.position.y, camera.position.z);
			}
			ImGui::End();
		}

		// This is where the "real" loop begins
		camera.Update(window, deltaTime);

		gfx->ClearRenderTargets();

		gfx->BindPipeline(meshPipeline);

		view = glm::lookAtLH(camera.position, camera.position + camera.front, camera.up);
		projection = glm::perspectiveFovLH_ZO(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 2000.0f);

		// Sponza
		gfx->BindVertexBuffer(sponza.meshVB);
		gfx->BindIndexBuffer(sponza.meshIB);
		for (auto& mesh : sponza.meshes)
		{
			model = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f)) * glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0));
			model *= mesh.matrix; // multiply it by the gltf model matrix
			sceneData.MVPMatrix = projection * view * model;
			sceneData.modelViewMatrix = view * model;
			sceneData.lightPosition = glm::vec3(view * glm::vec4(lightPosition, 1.0f));
			sceneData.normalMatrix = glm::transpose(glm::inverse(view * model));
			gfx->UpdateBuffer<SceneData>(mesh.meshCB, &sceneData, 1);
			gfx->BindConstantBuffer(1, mesh.meshCB);

			for (auto& submesh : mesh.submeshes)
			{
				if (sponza.materials[submesh.materialIndex] != UINT32_MAX)
					gfx->BindTexture2D(sponza.textures[sponza.textureIndices[sponza.materials[submesh.materialIndex]]]);

				gfx->DrawIndexed(submesh.indexCount, 1, submesh.indexStart);
			}
		}

		// This is where the "real" loop ends, do not write rendering stuff below this
		gfx->Render();
	}
}

void Destroy()
{
	sponza.Destroy();
	flightHelmet.Destroy();

	edelete gfx;

	edelete window;

	Log::Shutdown();
}


int EdenMain()
{
	Init();

	while (!window->IsCloseRequested())
	{
		ED_PROFILE_FRAME("MainThread");

		Update();
	}

	Destroy();

	return 0;
}

