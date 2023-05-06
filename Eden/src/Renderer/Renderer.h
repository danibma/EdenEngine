#pragma once

#define GLENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "RHI/DynamicRHI.h"
#include "Core/Camera.h"
#include "Renderer/Skybox.h"
#include "Scene/SceneSerializer.h"

namespace Eden
{
	class Window;

	// Renderer Data
	struct RendererData
	{
		//==================
		// Scene Data
		//==================
		struct SceneData
		{
			glm::mat4 view;
			glm::mat4 viewProjection;
			glm::vec4 viewPosition;
		};

		// TODO: add a validation when adding directional lights on the editor
		static constexpr int MAX_DIRECTIONAL_LIGHTS = 2;
		BufferRef directionalLightsBuffer;
		static constexpr int MAX_POINT_LIGHTS = 32;
		BufferRef pointLightsBuffer;

		std::unordered_map<const char*, PipelineRef> pipelines;

		// TODO: probably add these two to the camera
		glm::mat4 viewMatrix;
		glm::mat4 projectionMatrix;
		SceneData sceneData;
		BufferRef sceneDataCB;
		Camera camera;

		// Skybox
		SharedPtr<Skybox> skybox;
		bool bIsSkyboxEnabled = true;

		// Scene
		Scene* currentScene = nullptr;

		// Rendering
		RenderPassRef forwardPass;
		RenderPassRef deferredBasePass;
		RenderPassRef deferredLightingPass;
		RenderPassRef sceneComposite;
		RenderPassRef objectPickerPass; // Editor Only
		BufferRef quadBuffer; // used for passes that render a texture, like scene composite
		bool bIsDeferredEnabled = false;

		// Timers
		GPUTimerRef renderTimer;

		struct SceneSettings
		{
			float exposure = 0.6f;
		} sceneSettings;
		BufferRef sceneSettingsBuffer;

		glm::vec2 viewportSize;

		Window* window;
	};

	class Renderer
	{
	private:
		// Renderer Data
		static RendererData* m_Data;
		static bool m_IsRendererInitialized;

	private:
		static void UpdatePointLights();
		static void UpdateDirectionalLights();
		static void ObjectPickerPass();
		static void DeferredRenderingPass();
		static void ForwardRenderingPass();
		static void SceneCompositePass();

	public:
		static void Init(Window* window);
		static void BeginRender();
		static void Render();
		static void EndRender();
		static void Shutdown();
		static bool IsInitialized();

		// Scene
		static void PrepareScene();
		static void NewScene();
		static void OpenScene(const std::filesystem::path& path);
		static void OpenSceneDialog();
		static void SaveSceneAs();
		static void SaveScene();
		static Scene* GetCurrentScene();
		static void SetViewportSize(glm::vec2 size);
		static void SetViewportSize(float x, float y);
		static glm::vec2 GetViewportSize();
		static void SetCameraPosition(float x, float y); // this is only used when there's an editor
		static bool& IsSkyboxEnabled();
		static bool& IsDeferredRenderingEnabled();
		static void SetNewSkybox(const char* path);
		static RendererData::SceneSettings& GetSceneSettings();

		static GPUTimerRef GetRenderTimer();

		static glm::mat4 GetViewMatrix();
		static glm::mat4 GetProjectionMatrix();

		// This is used for the object picking, don't use it for anything else, use ReadPixelFromTexture
		static void GetPixel(uint32_t x, uint32_t y, glm::vec4& pixel);

		static TextureRef GetFinalImage();
		static TextureRef GetComputeTestOutputImage();
		static std::unordered_map<const char*, PipelineRef>& GetPipelines();

	};
}
