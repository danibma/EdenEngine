#include "Core/Window.h"
#include "Core/Memory.h"
#include "Core/Input.h"
#include "Core/Log.h"
#include "Profiling/Timer.h"
#include "Profiling/Profiler.h"
#include "Graphics/D3D12RHI.h"
#include "Core/Camera.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui/imgui.h>

#include "Scene/LightCasters.h"
#include "Scene/Model.h"

using namespace Eden;

Window* window;
D3D12RHI* rhi;

// Timer stuff
Timer timer;
float delta_time;

//==================
// Scene Data
//==================
struct SceneData
{
	glm::mat4 view;
	glm::mat4 view_projection;
	glm::vec4 view_position;
};

DirectionalLight directional_light;
Buffer directional_light_cb;
float directional_light_intensity = 0.5f;
std::vector<PointLight> point_lights;
Buffer point_lights_cb;

glm::mat4 model;
glm::mat4 view;
glm::mat4 projection;
glm::vec3 light_direction(0.0f, 10.0f, 0.0f);
SceneData scene_data;
Buffer scene_data_cb;
Camera camera;

Pipeline object_texture;
Pipeline object_simple;
Pipeline light_caster;
Model sponza;
Model flight_helmet;
Model basic_mesh;
Model floor_mesh;

// Skybox
Pipeline skybox;
Model skybox_cube;
Texture2D skybox_texture;
struct SkyboxData
{
	glm::mat4 view_projection_;
} skybox_data;
Buffer skybox_data_cb;

void Init()
{
	Log::Init();

#ifdef ED_DEBUG
	window = enew Window("Eden Engine[DEBUG]", 1600, 900);
#else
	window = enew Window("Eden Engine", 1600, 900);
#endif 

	rhi = enew D3D12RHI(window);
	rhi->EnableImGui();

	PipelineState object_textureDS;
	object_textureDS.enable_blending = true;
	object_texture = rhi->CreateGraphicsPipeline("object_texture", object_textureDS);
	object_simple = rhi->CreateGraphicsPipeline("object_simple");
	light_caster = rhi->CreateGraphicsPipeline("light_caster");

	PipelineState skybox_ds;
	skybox_ds.cull_mode = D3D12_CULL_MODE_NONE;
	skybox_ds.depth_func = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skybox_ds.min_depth = 1.0f;
	skybox = rhi->CreateGraphicsPipeline("skybox", skybox_ds);

	sponza.LoadGLTF(rhi, "assets/Models/DamagedHelmet/DamagedHelmet.glb");
	basic_mesh.LoadGLTF(rhi, "assets/Models/basic/sphere.glb");
	skybox_cube.LoadGLTF(rhi, "assets/Models/basic/cube.glb");
	floor_mesh.LoadGLTF(rhi, "assets/Models/basic/cube.glb");

	camera = Camera(window->GetWidth(), window->GetHeight());

	view = glm::lookAtLH(camera.position, camera.position + camera.front, camera.up);
	projection = glm::perspectiveFovLH_ZO(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 200.0f);
	model = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
	scene_data.view = view;
	scene_data.view_projection = projection * view;
	scene_data.view_position = glm::vec4(camera.position, 1.0f);

	scene_data_cb = rhi->CreateBuffer<SceneData>(&scene_data, 1);

	skybox_data.view_projection_ = projection * glm::mat4(glm::mat3(view));
	skybox_data_cb = rhi->CreateBuffer<SkyboxData>(&skybox_data, 1);

	// Lights
	directional_light.data.direction = glm::vec4(light_direction, 1.0f);
	directional_light.data.intensity = directional_light_intensity;
	directional_light_cb = rhi->CreateBuffer<DirectionalLight::DirectionalLightData>(&directional_light.data, 1);

	point_lights.emplace_back();
	point_lights.back().model.LoadGLTF(rhi, "assets/Models/basic/cube.glb");
	point_lights.back().data.position = glm::vec4(3.0f, 0.0f, -5.0f, 1.0f);
	point_lights.back().data.color = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
	point_lights.back().light_color_buffer = rhi->CreateBuffer<glm::vec4>(&point_lights.back().data.color, 1);

	point_lights.emplace_back();
	point_lights.back().model.LoadGLTF(rhi, "assets/Models/basic/cube.glb");
	point_lights.back().data.position = glm::vec4(-3.0f, 0.0f, -5.0f, 1.0f);
	point_lights.back().data.color = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	point_lights.back().light_color_buffer = rhi->CreateBuffer<glm::vec4>(&point_lights.back().data.color, 1);

	std::vector<PointLight::PointLightData> point_lights_data(point_lights.size());
	point_lights_data.emplace_back(point_lights[0].data);
	point_lights_data.emplace_back(point_lights[1].data);
	point_lights_cb = rhi->CreateBuffer<PointLight::PointLightData>(point_lights_data.data(), point_lights_data.size(), D3D12RHI::BufferCreateSRV::kCreateSRV);
}

bool open_debug_window = true;
bool skybox_enable = false;
void Update()
{
	ED_PROFILE_FUNCTION();

	window->UpdateEvents();

	// Update timers
	delta_time = (float)timer.ElapsedSeconds();
	timer.Record();

	if (!window->IsMinimized())
	{
		// Debug ImGui Window stuff
		if (Input::GetKeyDown(KeyCode::F3))
			open_debug_window = !open_debug_window;

		if (open_debug_window)
		{
			ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_NoCollapse);
			ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize("Press F3 to close this window!").x) * 0.5f); // center text
			ImGui::TextDisabled("Press F3 to close this window!");
			if (ImGui::CollapsingHeader("Timers", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("CPU frame time: %.3fms", delta_time * 1000.0f);
			}
			if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::DragFloat3("Directional Light Direction", (float*)&light_direction, 0.10f, 0.0f, 0.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::DragFloat("Directional Light Intensity", &directional_light_intensity, 0.1f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::Checkbox("Enable Skybox", &skybox_enable);
			}
			ImGui::End();
		}

		// This is where the "real" loop begins
		camera.Update(window, delta_time);

		rhi->ClearRenderTargets();

		view = glm::lookAtLH(camera.position, camera.position + camera.front, camera.up);
		projection = glm::perspectiveFovLH_ZO(glm::radians(70.0f), (float)window->GetWidth(), (float)window->GetHeight(), 0.1f, 2000.0f);

		// Sponza
		rhi->BindPipeline(object_texture);
		rhi->BindVertexBuffer(sponza.mesh_vb);
		rhi->BindIndexBuffer(sponza.mesh_ib);
		for (auto& mesh : sponza.meshes)
		{
			mesh.SetTranslation(-3, 0, 0);
			mesh.UpdateTransform();
			rhi->UpdateBuffer<glm::mat4>(mesh.transform_cb, &mesh.transform, 1);

			scene_data.view = view;
			scene_data.view_projection = projection * view;
			scene_data.view_position = glm::vec4(camera.position, 1.0f);

			directional_light.data.direction = glm::vec4(light_direction, 1.0f);
			directional_light.data.intensity = directional_light_intensity;
			rhi->UpdateBuffer<DirectionalLight>(directional_light_cb, &directional_light.data, 1);

			rhi->UpdateBuffer<SceneData>(scene_data_cb, &scene_data, 1);
			rhi->BindConstantBuffer("SceneData", scene_data_cb);
			rhi->BindConstantBuffer("Transform", mesh.transform_cb);
			rhi->BindConstantBuffer("DirectionalLightCB", directional_light_cb);
			rhi->BindShaderResource(point_lights_cb);

			for (auto& submesh : mesh.submeshes)
			{
				rhi->BindShaderResource(submesh.material_index);
				rhi->DrawIndexed(submesh.index_count, 1, submesh.index_start);
			}
		}

		// Basic mesh
		rhi->BindPipeline(object_simple);
		rhi->BindVertexBuffer(basic_mesh.mesh_vb);
		rhi->BindIndexBuffer(basic_mesh.mesh_ib);
		for (auto& mesh : basic_mesh.meshes)
		{
			mesh.SetTranslation(3, 0, 0);
			mesh.UpdateTransform();
			rhi->UpdateBuffer<glm::mat4>(mesh.transform_cb, &mesh.transform, 1);

			scene_data.view = view;
			scene_data.view_projection = projection * view;
			scene_data.view_position = glm::vec4(camera.position, 1.0f);

			directional_light.data.direction = glm::vec4(light_direction, 1.0f);
			directional_light.data.intensity = directional_light_intensity;
			rhi->UpdateBuffer<DirectionalLight>(directional_light_cb, &directional_light.data, 1);

			rhi->UpdateBuffer<SceneData>(scene_data_cb, &scene_data, 1);
			rhi->BindConstantBuffer("SceneData", scene_data_cb);
			rhi->BindConstantBuffer("Transform", mesh.transform_cb);
			rhi->BindConstantBuffer("DirectionalLightCB", directional_light_cb);
			rhi->BindShaderResource(point_lights_cb);
		
			for (auto& submesh : mesh.submeshes)
				rhi->DrawIndexed(submesh.index_count, 1, submesh.index_start);
		}

		// Draw floor
		rhi->BindVertexBuffer(floor_mesh.mesh_vb);
		rhi->BindIndexBuffer(floor_mesh.mesh_ib);
		for (auto& mesh : floor_mesh.meshes)
		{
			mesh.SetScale(10.0f, 0.2f, 10.0f);
			mesh.SetTranslation(0.0f, -2.0f, 0.0f);
			mesh.UpdateTransform();
			rhi->UpdateBuffer<glm::mat4>(mesh.transform_cb, &mesh.transform, 1);

			scene_data.view = view;
			scene_data.view_projection = projection * view;
			scene_data.view_position = glm::vec4(camera.position, 1.0f);

			directional_light.data.direction = glm::vec4(light_direction, 1.0f);
			directional_light.data.intensity = directional_light_intensity;
			rhi->UpdateBuffer<DirectionalLight>(directional_light_cb, &directional_light.data, 1);

			rhi->UpdateBuffer<SceneData>(scene_data_cb, &scene_data, 1);
			rhi->BindConstantBuffer("SceneData", scene_data_cb);
			rhi->BindConstantBuffer("Transform", mesh.transform_cb);
			rhi->BindConstantBuffer("DirectionalLightCB", directional_light_cb);
			rhi->BindShaderResource(point_lights_cb);

			for (auto& submesh : mesh.submeshes)
				rhi->DrawIndexed(submesh.index_count, 1, submesh.index_start);
		}

		// Draw point lights
		rhi->BindPipeline(light_caster);
		for (auto& point_light : point_lights)
		{
			rhi->BindVertexBuffer(point_light.model.mesh_vb);
			rhi->BindIndexBuffer(point_light.model.mesh_ib);

			for (auto& mesh : point_light.model.meshes)
			{
				mesh.SetTranslation(point_light.data.position.x, point_light.data.position.y, point_light.data.position.z);
				mesh.SetScale(0.2f, 0.2f, 0.2f);
				mesh.UpdateTransform();
				rhi->UpdateBuffer<glm::mat4>(mesh.transform_cb, &mesh.transform, 1);

				scene_data.view = view;
				scene_data.view_projection = projection * view;
				scene_data.view_position = glm::vec4(camera.position, 1.0f);

				directional_light.data.direction = glm::vec4(light_direction, 1.0f);
				directional_light.data.intensity = directional_light_intensity;
				rhi->UpdateBuffer<DirectionalLight>(directional_light_cb, &directional_light.data, 1);

				rhi->UpdateBuffer<SceneData>(scene_data_cb, &scene_data, 1);
				rhi->BindConstantBuffer("SceneData", scene_data_cb);
				rhi->BindConstantBuffer("Transform", mesh.transform_cb);
				rhi->BindConstantBuffer("LightColor", point_light.light_color_buffer);

				for (auto& submesh : mesh.submeshes)
					rhi->DrawIndexed(submesh.index_count, 1, submesh.index_start);
			}
		}
		

		// Skybox
		if (skybox_enable)
		{
			if (!skybox_texture.resource) // If the texture is not loaded yet, load it
			{
				skybox_texture = rhi->CreateTexture2D("assets/skyboxes/sky.hdr");
				skybox_texture.resource->SetName(L"Skybox texture");
			}

			rhi->BindPipeline(skybox);
			rhi->BindVertexBuffer(skybox_cube.mesh_vb);
			rhi->BindIndexBuffer(skybox_cube.mesh_ib);
			for (auto& mesh : skybox_cube.meshes)
			{
				skybox_data.view_projection_ = projection * glm::mat4(glm::mat3(view));
				rhi->UpdateBuffer<SkyboxData>(skybox_data_cb, &skybox_data, 1);
				rhi->BindConstantBuffer("SkyboxData", skybox_data_cb);

				for (auto& submesh : mesh.submeshes)
				{
					rhi->BindShaderResource(skybox_texture);
					rhi->DrawIndexed(submesh.index_count, 1, submesh.index_start);
				}
			}
		}

		// This is where the "real" loop ends, do not write rendering stuff below this
		rhi->Render();
	}
}

void Destroy()
{
	floor_mesh.Destroy();
	point_lights_cb.Release();
	directional_light_cb.Release();
	scene_data_cb.Release();
	skybox_data_cb.Release();
	skybox_cube.Destroy();
	skybox_texture.Release();
	sponza.Destroy();
	flight_helmet.Destroy();
	basic_mesh.Destroy();

	for (auto& point_light : point_lights)
		point_light.Destroy();

	edelete rhi;

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

