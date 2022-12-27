#pragma once

#define GLENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "RHI.h"
#include "Core/Camera.h"
#include "Graphics/Skybox.h"
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

		//==================
		// Compute Shader CB
		//==================
		struct ComputeData
		{
			alignas(16) glm::vec2 resolution;
			float time;
		} computeData;
		Buffer computeBuffer;
		Texture outputTexture;

		// TODO: add a validation when adding directional lights on the editor
		static constexpr int MAX_DIRECTIONAL_LIGHTS = 2;
		Buffer directionalLightsBuffer;
		static constexpr int MAX_POINT_LIGHTS = 32;
		Buffer pointLightsBuffer;

		std::unordered_map<const char*, Pipeline> pipelines;

		// TODO: probably add these two to the camera
		glm::mat4 viewMatrix;
		glm::mat4 projectionMatrix;
		SceneData sceneData;
		Buffer sceneDataCB;
		Camera camera;

		// Skybox
		SharedPtr<Skybox> skybox;
		bool bIsSkyboxEnabled = true;

		// Scene
		Scene* currentScene = nullptr;

		// Rendering
		RenderPass gBuffer;
		RenderPass sceneComposite;
		Buffer quadBuffer;
		RenderPass objectPickerPass; // Editor Only

		// Timers
		GPUTimer renderTimer; // TODO: make a vector of this timers inside the renderer class
		GPUTimer computeTimer;

		struct SceneSettings
		{
			float exposure = 0.6f;
		} sceneSettings;
		Buffer sceneSettingsBuffer;

		glm::vec2 viewportSize;

		Window* window;
	};

	class Renderer
	{
	private:
		static void UpdatePointLights();
		static void UpdateDirectionalLights();
		static void ObjectPickerPass();
		static void MainColorPass();
		static void SceneCompositePass();

	public:
		static void Init(Window* window);
		static void BeginRender();
		static void Render();
		static void EndRender();
		static void Shutdown();

		static API GetCurrentAPI();

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
		static void SetNewSkybox(const char* path);
		static RendererData::SceneSettings& GetSceneSettings();

		static GPUTimer& GetRenderTimer();
		static GPUTimer& GetComputeTimer();

		static glm::mat4 GetViewMatrix();
		static glm::mat4 GetProjectionMatrix();

		// Renderer API
		static void CreateRenderpass(RenderPass* renderPass, RenderPassDesc* desc);
		static void BeginRenderPass(RenderPass* renderPass);
		static void EndRenderPass(RenderPass* renderPass);
		static void SetSwapchainTarget(RenderPass* renderPass);
		static void EnableImGui();
		static void ReadPixelFromTexture(uint32_t x, uint32_t y, Texture* texture, glm::vec4& pixel);
		static uint64_t GetTextureID(Texture* texture);
		static void ReloadPipeline(Pipeline* pipeline);
		static void EnsureMsgResourceState(Texture* resource, ResourceState destResourceState);
		static void CreateTexture(Texture* texture, std::string path, bool bGenerateMips);
		static void CreateTexture(Texture* texture, TextureDesc* desc);
		static void CreateBuffer(Buffer* buffer, BufferDesc* desc, const void* initial_data);
		static void SetName(Resource* child, std::string& name);
		static void BindPipeline(Pipeline* pipeline);
		static void BindVertexBuffer(Buffer* vertexBuffer);
		static void BindIndexBuffer(Buffer* indexBuffer);
		static void BindParameter(const std::string& parameterName, Buffer* buffer);
		static void BindParameter(const std::string& parameterName, Texture* texture, TextureUsage usage = kReadOnly);
		static void BindParameter(const std::string& parameterName, void* data, size_t size); // Use only for constants
		static void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t startVertexLocation = 0, uint32_t startInstanceLocation = 0);
		static void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t startIndexLocation = 0, uint32_t baseVertexLocation = 0, uint32_t startInstanceLocation = 0);
		static void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

		// This is used for the object picking, don't use it for anything else, use ReadPixelFromTexture
		static void GetPixel(uint32_t x, uint32_t y, glm::vec4& pixel);

		static Texture& GetFinalImage();
		static Texture& GetComputeTestOutputImage();
		static std::unordered_map<const char*, Pipeline>& GetPipelines();

	};
}
