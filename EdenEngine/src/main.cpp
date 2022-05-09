#include "Core/Window.h"
#include "Core/Memory.h"
#include "Core/Input.h"
#include "Core/Log.h"
#include "Profiling/Timer.h"
#include "Profiling/Profiler.h"
#include "Graphics/GraphicsDevice.h"
#include "Core/Camera.h"

#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
};

glm::mat4 model;
glm::mat4 view;
glm::mat4 projection;
SceneData sceneData;
Camera camera;

std::vector<Vertex> triangleVertices;

void Init()
{
	Log::Init();

#ifdef ED_DEBUG
	window = enew Window("Eden Engine(DEBUG)", 1600, 900);
#else
	window = enew Window("Eden Engine", 1600, 900);
#endif

	gfx = enew GraphicsDevice(window->GetHandle(), window->GetWidth(), window->GetHeight());

	gfx->CreateGraphicsPipeline("basic");

	triangleVertices =
	{
		{ { 0.0f, 1.0f, 0.0f }, { 0.5f, 0.0f } },
		{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
		{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } }
	};

	camera = Camera(window->GetWidth(), window->GetHeight());

	view = glm::lookAtRH(camera.position, camera.position + camera.front, camera.up);
	projection = glm::perspectiveFovRH(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 200.0f);
	model = glm::mat4(1.0f);
	sceneData.MVPMatrix = projection * view * model;

	gfx->CreateVertexBuffer(triangleVertices.data(), (uint32_t)triangleVertices.size() * sizeof(Vertex), sizeof(Vertex));
	gfx->CreateTexture2D("assets/container2.png");
	gfx->CreateConstantBuffer(sceneData);
}

uint32_t frameNumber;

void Update()
{
	ED_PROFILE_FUNCTION();

	window->UpdateEvents();

	// Update timers
	deltaTime = (float)timer.ElapsedSeconds();
	timer.Record();

	if (!window->IsMinimized())
	{
		camera.Update(deltaTime);

		view = glm::lookAtRH(camera.position, camera.position + camera.front, camera.up);
		projection = glm::perspectiveFovRH(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 200.0f);
		model = glm::rotate(glm::mat4(1.0f), glm::radians(frameNumber + 0.4f), glm::vec3(0, 1, 0));
		sceneData.MVPMatrix = projection * view * model;

		gfx->UpdateConstantBuffer(sceneData);
		gfx->Render();
	}

	frameNumber++;
}

void Destroy()
{
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

