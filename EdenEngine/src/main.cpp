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

#include <imgui/imgui.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>


using namespace Eden;

Window* window;
GraphicsDevice* gfx;

// Timer stuff
Timer timer;
float deltaTime;

// scene data
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

glm::mat4 model;
glm::mat4 view;
glm::mat4 projection;
glm::vec3 lightPosition(0.0f);
SceneData sceneData;
Camera camera;

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

void Init()
{
	Log::Init();

#ifdef ED_DEBUG
	window = enew Window("Eden Engine[DEBUG]", 1600, 900);
#else
	window = enew Window("Eden Engine", 1600, 900);
#endif

	gfx = enew GraphicsDevice(window);

	gfx->CreateGraphicsPipeline("basic");

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
	LoadObj("assets/survival_guitar_backpack/obj/sgb.obj");
#endif

	camera = Camera(window->GetWidth(), window->GetHeight());

	view = glm::lookAtLH(camera.position, camera.position + camera.front, camera.up);
	projection = glm::perspectiveFovLH_ZO(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 200.0f);
	model = glm::scale(glm::mat4(1.0f), glm::vec3(0.01f));
	sceneData.MVPMatrix = projection * view * model;
	sceneData.modelViewMatrix = view * model;
	sceneData.lightPosition = lightPosition;

	meshVB = gfx->CreateVertexBuffer(meshVertices.data(), (uint32_t)meshVertices.size(), sizeof(Vertex));
	meshIB = gfx->CreateIndexBuffer(meshIndices.data(), (uint32_t)meshIndices.size(), sizeof(uint32_t));
	//gfx->CreateTexture2D("assets/container2.png");
	gfx->CreateConstantBuffer(sceneData);

	gfx->vertexBuffer = meshVB;
	gfx->indexBuffer = meshIB;

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
			ImGui::Separator();
			ImGui::Text("CPU time: %.2fms", deltaTime * 1000.0f);
			ImGui::Separator();
			ImGui::Text("Mesh Vertices: %d", meshVertices.size());
			ImGui::Text("Mesh Indices: %d", meshIndices.size());
			ImGui::Separator();
			ImGui::DragFloat3("Light Position", (float*)&lightPosition, 1.0f, 0.0f, 0.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::Text("Camera Position: %f, %f, %f", camera.position.x, camera.position.y, camera.position.z);
			ImGui::End();
		}

		// This is where the "real" loop begins
		camera.Update(window, deltaTime);

		view = glm::lookAtLH(camera.position, camera.position + camera.front, camera.up);
		projection = glm::perspectiveFovLH_ZO(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 200.0f);
		model = glm::scale(glm::mat4(1.0f), glm::vec3(0.01f));
		sceneData.MVPMatrix = projection * view * model;
		sceneData.modelViewMatrix = view * model;
		sceneData.lightPosition = glm::vec3(view * glm::vec4(lightPosition, 1.0f));
		sceneData.normalMatrix = glm::transpose(glm::inverse(view * model));

		gfx->UpdateConstantBuffer(sceneData);

		// This is where the "real" loop ends, do not write rendering stuff below this
		gfx->Render();
	}
}

void Destroy()
{
	meshVB.allocation->Release();
	meshIB.allocation->Release();
	meshTextureDiffuse.allocation->Release();
	meshTextureNormal.allocation->Release();

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

