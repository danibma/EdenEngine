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

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

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
glm::vec3 lightPosition(0.0f, 7.0f, -8.0f);
SceneData sceneData;
Camera camera;

Pipeline meshPipeline;
Texture2D meshTextureDiffuse;
Texture2D meshTextureNormal;
VertexBuffer meshVB;
IndexBuffer meshIB;
std::vector<Vertex> meshVertices;
std::vector<uint32_t> meshIndices;

void LoadObj(std::filesystem::path file)
{
	//attrib will contain the vertex arrays of the file
	tinyobj::attrib_t attrib;
	//shapes contains the info for each separate object in the file
	std::vector<tinyobj::shape_t> shapes;
	//materials contains the information about the material of each shape, but we won't use it.
	std::vector<tinyobj::material_t> materials;

	//error and warning output from the load function
	std::string warn;
	std::string err;

	std::string parentPath = file.parent_path().string() + "/";

	tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, file.string().c_str(), parentPath.c_str());
	if (!warn.empty())
	{
		ED_LOG_ERROR("{}", warn);
	}

	if (!err.empty())
	{
		ED_LOG_FATAL("{}", err);
		return;
	}

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};

	// Loop over shapes
	for (const auto& shape : shapes)
	{
		for (const auto& index : shape.mesh.indices)
		{
			Vertex newVert = {};

			// vertex position
			newVert.position.x =  attrib.vertices[3 * index.vertex_index + 0];
			newVert.position.y =  attrib.vertices[3 * index.vertex_index + 1];
			newVert.position.z = -attrib.vertices[3 * index.vertex_index + 2];

			// vertex normal
			if (attrib.normals.size() > 0)
			{
				newVert.normal.x =  attrib.normals[3 * index.normal_index + 0];
				newVert.normal.y =  attrib.normals[3 * index.normal_index + 1];
				newVert.normal.z = -attrib.normals[3 * index.normal_index + 2];
			}
			
			// vertex uv
			if (attrib.texcoords.size() > 0)
			{
				newVert.uv.x = attrib.texcoords[2 * index.texcoord_index + 0];
				newVert.uv.y = 1 - attrib.texcoords[2 * index.texcoord_index + 1];
			}
			

			if (uniqueVertices.count(newVert) == 0) {
				uniqueVertices[newVert] = static_cast<uint32_t>(meshVertices.size());
				meshVertices.push_back(newVert);
			}

			meshIndices.push_back(uniqueVertices[newVert]);
		}
	}

	// TODO(Daniel): Right now this is only loading the first diffuse texture,
	//				 make it so it loads every texture
	meshTextureDiffuse = gfx->CreateTexture2D(parentPath + materials[0].diffuse_texname);
	meshTextureNormal = gfx->CreateTexture2D(parentPath + materials[0].bump_texname);
}

void LoadGLTF(std::filesystem::path file)
{
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;
	bool result = false;

	if(file.extension() == ".gltf")
		result = loader.LoadASCIIFromFile(&model, &err, &warn, file.string());
	else if (file.extension() == ".glb")
		result = loader.LoadBinaryFromFile(&model, &err, &warn, file.string());

	if (!warn.empty())
		ED_LOG_WARN("{}", warn.c_str());

	if (!err.empty())
		ED_LOG_ERROR("{}", err.c_str());

	if (!result) 
	{
		ED_LOG_ERROR("Failed to parse GLTF Model!");
		return;
	}

	std::string parentPath = file.parent_path().string() + "/";

	for (auto const& gltf_mesh : model.meshes)
	{
		for (size_t p = 0; p < gltf_mesh.primitives.size(); ++p)
		{
			auto& glTFPrimitive = gltf_mesh.primitives[p];
			uint32_t vertexStart = (uint32_t)meshVertices.size();
			uint32_t firstIndex = (uint32_t)meshIndices.size();
			uint32_t indexCount = 0;

			// Vertices
			{
				const float* positionBuffer;
				const float* normalsBuffer;
				const float* uvsBuffer;
				size_t vertexCount = 0;

				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = model.accessors[glTFPrimitive.attributes.find("POSITION")->second];
					const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
					positionBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					vertexCount = accessor.count;
				}

				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = model.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
					normalsBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				// Get buffer data for vertex texture coordinates
				// glTF supports multiple sets, we only load the first one
				if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = model.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
					uvsBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				for (size_t v = 0; v < vertexCount; ++v)
				{
					Vertex newVert = {};
					newVert.position = glm::make_vec3(&positionBuffer[v * 3]);
					newVert.position.z = -newVert.position.z;
					newVert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
					newVert.normal.z = -newVert.normal.z;
					newVert.uv = uvsBuffer ? glm::make_vec2(&uvsBuffer[v * 2]) : glm::vec2(0.0f);
					meshVertices.emplace_back(newVert);
				}
			}

			// Indices
			{
				const tinygltf::Accessor& accessor = model.accessors[glTFPrimitive.indices];
				const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

				indexCount += static_cast<uint32_t>(accessor.count);

				// glTF supports different component types of indices
				switch (accessor.componentType) {
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
				{
					uint32_t* buf = new uint32_t[accessor.count];
					memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint32_t));
					for (size_t index = 0; index < accessor.count; index++) {
						meshIndices.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
				{
					uint16_t* buf = new uint16_t[accessor.count];
					memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
					for (size_t index = 0; index < accessor.count; index++) {
						meshIndices.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
				{
					uint8_t* buf = new uint8_t[accessor.count];
					memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
					for (size_t index = 0; index < accessor.count; index++) {
						meshIndices.push_back(buf[index] + vertexStart);
					}
					break;
				}
				default:
					ED_LOG_ERROR("Index component type {} not supported!");
					return;
				}
			}
		}
	}

	// Load texture
	// TODO(Daniel): Right now this is only loading the first diffuse texture,
	//				 make it so it loads every texture
	meshTextureDiffuse = gfx->CreateTexture2D(parentPath + model.images[0].uri);
	meshTextureNormal = gfx->CreateTexture2D(parentPath + model.images[2].uri);
}

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

#if 0
	meshVertices =
	{
		{ { -0.5f,  0.5f, 0.5f }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }}, // top left
		{ {  0.5f, -0.5f, 0.5f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }}, // bottom right
		{ { -0.5f, -0.5f, 0.5f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }}, // bottom left
		{ {  0.5f,  0.5f, 0.5f }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }}, // top right
	};

	meshIndices = 
	{
		0, 1, 2, // first triangle
		0, 3, 1 // second triangle
	};
#else
	LoadGLTF("assets/scifihelmet/scifihelmet.gltf");
 	//LoadObj("assets/survival_guitar_backpack/obj/sgb.obj");
#endif

	camera = Camera(window->GetWidth(), window->GetHeight());

	view = glm::lookAtLH(camera.position, camera.position + camera.front, camera.up);
	projection = glm::perspectiveFovLH_ZO(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 200.0f);
	model = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
	sceneData.MVPMatrix = projection * view * model;
	sceneData.modelViewMatrix = view * model;
	sceneData.lightPosition = lightPosition;

	meshVB = gfx->CreateVertexBuffer(meshVertices.data(), (uint32_t)meshVertices.size(), sizeof(Vertex));
	meshIB = gfx->CreateIndexBuffer(meshIndices.data(), (uint32_t)meshIndices.size(), sizeof(uint32_t));

	gfx->BindVertexBuffer(meshVB);
	gfx->BindIndexBuffer(meshIB);

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
				ImGui::Text("CPU time: %.2fms", deltaTime * 1000.0f);
			}
			if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("Mesh Vertices: %d", meshVertices.size());
				ImGui::Text("Mesh Indices: %d", meshIndices.size());
				ImGui::DragFloat3("Light Position", (float*)&lightPosition, 1.0f, 0.0f, 0.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::Text("Camera Position: %f, %f, %f", camera.position.x, camera.position.y, camera.position.z);
			}
			ImGui::End();
		}

		// This is where the "real" loop begins
		camera.Update(window, deltaTime);

		view = glm::lookAtLH(camera.position, camera.position + camera.front, camera.up);
		projection = glm::perspectiveFovLH_ZO(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 2000.0f);
		model = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
		sceneData.MVPMatrix = projection * view * model;
		sceneData.modelViewMatrix = view * model;
		sceneData.lightPosition = glm::vec3(view * glm::vec4(lightPosition, 1.0f));
		sceneData.normalMatrix = glm::transpose(glm::inverse(view * model));

		gfx->SetPipelineParameter(meshPipeline, "SceneData", sceneData);

		gfx->BindPipeline(meshPipeline);
		gfx->DrawIndexed(meshIB.indexCount);

		// This is where the "real" loop ends, do not write rendering stuff below this
		gfx->Render();
	}
}

void Destroy()
{
	meshPipeline.Release();
	meshVB.Release();
	meshIB.Release();
	meshTextureDiffuse.Release();
	meshTextureNormal.Release();

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

