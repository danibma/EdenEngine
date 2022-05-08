#include "Core/Window.h"
#include "Core/Memory.h"
#include "Core/Input.h"
#include "Core/Log.h"
#include "Profiling/Timer.h"
#include "Profiling/Profiler.h"
#include "Graphics/GraphicsDevice.h"

#include <algorithm>

#include <glm/glm.hpp>

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
		{ { 0.0f, 0.25f * window->GetAspectRatio(), 0.0f }, { 0.5f, 0.0f } },
		{ { 0.25f, -0.25f * window->GetAspectRatio(), 0.0f }, { 1.0f, 1.0f } },
		{ { -0.25f, -0.25f * window->GetAspectRatio(), 0.0f }, { 0.0f, 1.0f } }
	};

	gfx->CreateVertexBuffer(triangleVertices.data(), (uint32_t)triangleVertices.size() * sizeof(Vertex), sizeof(Vertex));
	gfx->CreateTexture2D("assets/container2.png");
}

void Update()
{
	ED_PROFILE_FUNCTION();

	window->UpdateEvents();

	// Update timers
	deltaTime = (float)timer.ElapsedSeconds();
	timer.Record();

	if (!window->IsMinimized())
	{
		gfx->Render();

		if (Input::GetKeyDown(KeyCode::A))
		{
			ED_LOG_INFO("Key Pressed!");
		}
		else if (Input::GetKeyUp(KeyCode::A))
		{
			ED_LOG_INFO("Key Up!");
		}
	}
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

