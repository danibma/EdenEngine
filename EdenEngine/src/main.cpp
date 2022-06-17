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

#include "Scene/LightCasters.h"
#include "Scene/Model.h"
#include "Core/Application.h"
#include "Core/UI.h"

using namespace Eden;

class EdenApplication : public Application
{
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
	glm::vec3 light_direction = glm::vec3(0.0f, 10.0f, 0.0f);
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

	bool skybox_enable = false;

	// UI
	bool open_debug_window = true;
	bool open_entity_properties = true;
	bool show_ui = true;
public:
	EdenApplication() = default;

	void OnInit() override
	{
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
		point_lights.back().data.position = glm::vec4(3.0f, 3.0f, -5.0f, 1.0f);
		point_lights.back().data.color = glm::vec4(0.0f, 1.0f, 1.0f, 0.0f);
		point_lights.back().light_color_buffer = rhi->CreateBuffer<glm::vec4>(&point_lights.back().data.color, 1);

		point_lights.emplace_back();
		point_lights.back().model.LoadGLTF(rhi, "assets/Models/basic/cube.glb");
		point_lights.back().data.position = glm::vec4(-3.0f, 3.0f, -5.0f, 1.0f);
		point_lights.back().data.color = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
		point_lights.back().light_color_buffer = rhi->CreateBuffer<glm::vec4>(&point_lights.back().data.color, 1);

		std::vector<PointLight::PointLightData> point_lights_data(point_lights.size());
		point_lights_data.emplace_back(point_lights[0].data);
		point_lights_data.emplace_back(point_lights[1].data);
		point_lights_cb = rhi->CreateBuffer<PointLight::PointLightData>(point_lights_data.data(), point_lights_data.size(), D3D12RHI::BufferCreateSRV::kCreateSRV);
	}

	void UI_Toolbar()
	{
		const float toolbar_size = 1.0f;
		bool show_help_popup = false;

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y));
		ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, toolbar_size));
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags window_flags = 0
			| ImGuiWindowFlags_NoDocking
			| ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoScrollbar
			| ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_MenuBar
			;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
		ImGui::Begin("TOOLBAR", NULL, window_flags);
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Windows"))
			{
				ImGui::MenuItem("Debug", NULL, &open_debug_window);
				ImGui::MenuItem("Entity Properties", 0, &open_entity_properties);

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Help"))
			{
				if (ImGui::MenuItem("Help"))
					show_help_popup = !show_help_popup;

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		// Help Popup
		if (show_help_popup)
			ImGui::OpenPopup("help_popup");
		if (ImGui::BeginPopupModal("help_popup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar))
		{
			UI::CenterWindow();
			ImGui::Dummy(ImVec2(0, 10));
			const char* string = "Press F3 to Hide the UI";
			UI::Text(string, UI::Align::Center);
			ImGui::Dummy(ImVec2(0, 20));
			if (UI::Button("Close", UI::Align::Center))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		ImGui::End();
	}

	void UI_EntityProperties()
	{
		ImGui::Begin("Entity Properties", &open_entity_properties, ImGuiWindowFlags_NoCollapse);
		ImGui::DragFloat3("Direction", (float*)&light_direction, 0.10f, 0.0f, 0.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("Intensity", &directional_light_intensity, 0.1f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::Checkbox("Enable Skybox", &skybox_enable);
		ImGui::End();
	}

	void UI_DebugWindow()
	{
		ImGui::Begin("Debug", &open_debug_window, ImGuiWindowFlags_NoCollapse);

		ImGui::Text("CPU frame time: %.3fms", delta_time * 1000.0f);

		ImGui::End();
	}

	void UI_Render()
	{
		UI_Toolbar();
		if (open_debug_window)
			UI_DebugWindow();
		if (open_entity_properties)
			UI_EntityProperties();
	}

	void OnUpdate() override
	{
		ED_PROFILE_FUNCTION();

		rhi->BeginRender();

		// Show UI or not
		if (Input::GetKeyDown(KeyCode::F3))
			show_ui = !show_ui;
		if (show_ui)
			UI_Render();

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

		rhi->EndRender();
		rhi->Render();
	}


	void OnDestroy() override
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
	}

};
EdenApplication* app;

int EdenMain()
{
	app = enew EdenApplication();
	app->Run();
	edelete app;

	return 0;
}

