#include "Core/Window.h"
#include "Core/Memory.h"
#include "Core/Input.h"
#include "Core/Log.h"
#include "Profiling/Timer.h"
#include "Profiling/Profiler.h"
#include "Graphics/GraphicsDevice.h"
#include "Core/Camera.h"
#include "Utilities/Utils.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui/imgui.h>

#include "Scene/Model.h"

using namespace Eden;

Window* window;
GraphicsDevice* gfx;

// Timer stuff
Timer timer;
float deltaTime;

//==================
// Scene Data
//==================
struct SceneData
{
	//glm::mat4 MVPMatrix;
	//glm::mat4 modelViewMatrix;
	//// This matrix is used to fix the problem of a uniform scale only changing the normal's magnitude and not it's direction
	//glm::mat4 normalMatrix;
	//glm::vec3 lightPosition;

	glm::mat4 view;
	glm::mat4 viewProjection;
	glm::vec4 lightPosition;
};

glm::mat4 model;
glm::mat4 view;
glm::mat4 projection;
glm::vec3 lightPosition(0.0f, 7.0f, 0.0f);
SceneData sceneData;
Buffer sceneDataCB;
Camera camera;

Pipeline basic_texture;
Pipeline basic;
Model sponza;
Model flightHelmet;
Model basicMesh;

// Skybox
Pipeline skybox;
Model skyboxCube;
Texture2D skyboxTexture;
struct SkyboxData
{
	glm::mat4 viewProjection;
} skyboxData;
Buffer skyboxDataCB;

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

	PipelineState basic_textureDS;
	basic_textureDS.enableBlending = true;
	basic_texture = gfx->CreateGraphicsPipeline("basic_texture", basic_textureDS);

	basic = gfx->CreateGraphicsPipeline("basic");

	PipelineState skyboxDS;
	skyboxDS.cullMode = D3D12_CULL_MODE_NONE;
	skyboxDS.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyboxDS.minDepth = 1.0f;
	skybox = gfx->CreateGraphicsPipeline("skybox", skyboxDS);

	sponza.LoadGLTF(gfx, "assets/Models/DamagedHelmet/DamagedHelmet.glb");
	basicMesh.LoadGLTF(gfx, "assets/Models/basic/sphere.glb");
	skyboxCube.LoadGLTF(gfx, "assets/Models/basic/cube.glb");

	camera = Camera(window->GetWidth(), window->GetHeight());

	view = glm::lookAtLH(camera.position, camera.position + camera.front, camera.up);
	projection = glm::perspectiveFovLH_ZO(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 200.0f);
	model = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
	sceneData.view = view;
	sceneData.viewProjection = projection * view;
	sceneData.lightPosition = glm::vec4(lightPosition, 1.0f);

	sceneDataCB = gfx->CreateBuffer<SceneData>(&sceneData, 1);

	skyboxData.viewProjection = projection * glm::mat4(glm::mat3(view));
	skyboxDataCB = gfx->CreateBuffer<SkyboxData>(&skyboxData, 1);
}

bool openDebugWindow = true;
bool skyboxEnable = false;
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
				ImGui::DragFloat3("Light Position", (float*)&lightPosition, 1.0f, 0.0f, 0.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::Checkbox("Enable Skybox", &skyboxEnable);
			}
			ImGui::End();
		}

		// This is where the "real" loop begins
		camera.Update(window, deltaTime);

		gfx->ClearRenderTargets();

		view = glm::lookAtLH(camera.position, camera.position + camera.front, camera.up);
		projection = glm::perspectiveFovLH_ZO(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 2000.0f);

		// Sponza
		gfx->BindPipeline(basic_texture);
		gfx->BindVertexBuffer(sponza.meshVB);
		gfx->BindIndexBuffer(sponza.meshIB);
		for (auto& mesh : sponza.meshes)
		{
			mesh.SetTranslation(-3, 0, 0);
			mesh.UpdateTransform();
			gfx->UpdateBuffer<glm::mat4>(mesh.transformCB, &mesh.transform, 1);

			sceneData.view = view;
			sceneData.viewProjection = projection * view;
			sceneData.lightPosition = view * glm::vec4(lightPosition, 1.0f);
			gfx->UpdateBuffer<SceneData>(sceneDataCB, &sceneData, 1);
			gfx->BindConstantBuffer("SceneData", sceneDataCB);
			gfx->BindConstantBuffer("Transform", mesh.transformCB);

			for (auto& submesh : mesh.submeshes)
			{
				gfx->BindMaterial(submesh.materialIndex);
				gfx->DrawIndexed(submesh.indexCount, 1, submesh.indexStart);
			}
		}

		// Basic mesh
		gfx->BindPipeline(basic);
		gfx->BindVertexBuffer(basicMesh.meshVB);
		gfx->BindIndexBuffer(basicMesh.meshIB);
		for (auto& mesh : basicMesh.meshes)
		{
			mesh.SetTranslation(3, 0, 0);
			mesh.UpdateTransform();
			gfx->UpdateBuffer<glm::mat4>(mesh.transformCB, &mesh.transform, 1);

			sceneData.view = view;
			sceneData.viewProjection = projection * view;
			sceneData.lightPosition = view * glm::vec4(lightPosition, 1.0f);
			gfx->UpdateBuffer<SceneData>(sceneDataCB, &sceneData, 1);
			gfx->BindConstantBuffer("SceneData", sceneDataCB);
			gfx->BindConstantBuffer("Transform", mesh.transformCB);
		
			for (auto& submesh : mesh.submeshes)
				gfx->DrawIndexed(submesh.indexCount, 1, submesh.indexStart);
		}

		// Skybox
		if (skyboxEnable)
		{
			if (!skyboxTexture.resource) // If the texture is not loaded yet, load it
			{
				skyboxTexture = gfx->CreateTexture2D("assets/skyboxes/sky.hdr");
				skyboxTexture.resource->SetName(L"Skybox texture");
			}

			gfx->BindPipeline(skybox);
			gfx->BindVertexBuffer(skyboxCube.meshVB);
			gfx->BindIndexBuffer(skyboxCube.meshIB);
			for (auto& mesh : skyboxCube.meshes)
			{
				skyboxData.viewProjection = projection * glm::mat4(glm::mat3(view));
				gfx->UpdateBuffer<SkyboxData>(skyboxDataCB, &skyboxData, 1);
				gfx->BindConstantBuffer("SkyboxData", skyboxDataCB);

				for (auto& submesh : mesh.submeshes)
				{
					gfx->BindMaterial(skyboxTexture);
					gfx->DrawIndexed(submesh.indexCount, 1, submesh.indexStart);
				}
			}
		}

		// This is where the "real" loop ends, do not write rendering stuff below this
		gfx->Render();
	}
}

void Destroy()
{
	sceneDataCB.Release();
	skyboxDataCB.Release();
	skyboxCube.Destroy();
	skyboxTexture.Release();
	sponza.Destroy();
	flightHelmet.Destroy();
	basicMesh.Destroy();

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

