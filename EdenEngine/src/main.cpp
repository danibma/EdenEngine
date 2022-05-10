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

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_dx12.h>
#include <imgui/backends/imgui_impl_win32.h>
#include "Utilities/Utils.h"

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
	window = enew Window("Eden Engine[DEBUG]", 1600, 900);
#else
	window = enew Window("Eden Engine", 1600, 900);
#endif

	gfx = enew GraphicsDevice(window);

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
bool openDebugWindow = false;

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
		model = glm::rotate(glm::mat4(1.0f), glm::radians(frameNumber + 0.4f), glm::vec3(0, 1, 0));
		sceneData.MVPMatrix = projection * view * model;

		gfx->UpdateConstantBuffer(sceneData);

		// This is where the "real" loop ends, do not write rendering stuff below this
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

