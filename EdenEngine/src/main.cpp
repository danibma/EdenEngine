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

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
};

glm::mat4 model;
glm::mat4 view;
glm::mat4 projection;
SceneData sceneData;
Camera camera;

VertexBuffer meshVB;
IndexBuffer meshIB;
std::vector<Vertex> meshVertices;
std::vector<uint32_t> meshIndices;

void LoadObj(const char* file)
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

	tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, file);
	if (!warn.empty())
	{
		ED_LOG_ERROR("{}", warn);
	}

	if (!err.empty())
	{
		ED_LOG_FATAL("{}", err);
		return;
	}

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++)
	{
		// Loop over faces(polygon)
		size_t indexOffset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
		{
			//hardcode loading to triangles
			int fv = 3;

			// Loop over vertices in the face
			for (size_t v = 0; v < fv; v++)
			{
				// access to vertex
				tinyobj::index_t idx = shapes[s].mesh.indices[indexOffset + v];

				//vertex position
				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
				//vertex normal
				tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
				tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
				tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

				if (idx.texcoord_index < 0) idx.texcoord_index = 0;
				tinyobj::real_t ux = attrib.texcoords[2 * idx.texcoord_index + 0];
				tinyobj::real_t uy = attrib.texcoords[2 * idx.texcoord_index + 1];

				Vertex new_vert;
				//copy it into our vertex
				new_vert.position.x = vx;
				new_vert.position.y = vy;
				new_vert.position.z = vz;

				new_vert.normal.x = nx;
				new_vert.normal.y = ny;
				new_vert.normal.z = nz;

				new_vert.uv.x = ux;
				new_vert.uv.y = 1 - uy; //do the 1-y on the uv.y because Vulkan UV coordinates work like that.

				//we are setting the vertex color as the vertex normal. This is just for display purposes
				/*auto id = shapes[s].mesh.material_ids[f];
				if (id > -1)
					new_vert.color = glm::vec3(materials[id].diffuse[0], materials[id].diffuse[1], materials[id].diffuse[2]);
				else
					new_vert.color = glm::vec3(1, 1, 1);`*/

				meshVertices.push_back(new_vert);

			}
			indexOffset += fv;
		}

	}
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

#if 1
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
	LoadObj("assets/monkey_flat.obj");
#endif

	camera = Camera(window->GetWidth(), window->GetHeight());

	view = glm::lookAtRH(camera.position, camera.position + camera.front, camera.up);
	projection = glm::perspectiveFovRH(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 200.0f);
	model = glm::mat4(1.0f);
	sceneData.MVPMatrix = projection * view * model;

	meshVB = gfx->CreateVertexBuffer(meshVertices.data(), (uint32_t)meshVertices.size(), sizeof(Vertex));
	meshIB = gfx->CreateIndexBuffer(meshIndices.data(), (uint32_t)meshIndices.size(), sizeof(uint32_t));
	gfx->CreateTexture2D("assets/container2.png");
	gfx->CreateConstantBuffer(sceneData);

	gfx->vertexBuffer = meshVB;
	gfx->indexBuffer = meshIB;
}

uint32_t frameNumber;
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
			ImGui::End();
		}

		// This is where the "real" loop begins
		camera.Update(window, deltaTime);

		view = glm::lookAtRH(camera.position, camera.position + camera.front, camera.up);
		projection = glm::perspectiveFovRH(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 200.0f);
		//model = glm::rotate(glm::mat4(1.0f), glm::radians(frameNumber + 0.4f), glm::vec3(0, 1, 0));
		sceneData.MVPMatrix = projection * view * model;

		gfx->UpdateConstantBuffer(sceneData);

		// This is where the "real" loop ends, do not write rendering stuff below this
		gfx->Render();
	}

	frameNumber++;
}

void Destroy()
{
	meshVB.allocation->Release();
	meshIB.allocation->Release();

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

