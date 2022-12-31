#pragma once

#include "Graphics/RHI.h"

#ifdef ED_PLATFORM_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <Volk/volk.h>

namespace Eden
{
	class VulkanRHI final : public IRHI
	{
		VkInstance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessenger;
		VkPhysicalDevice m_PhysicalDevice;
		VkDevice m_Device;
		VkSurfaceKHR m_Surface;
		uint32_t m_GraphicsQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	public:
		virtual GfxResult Init(Window* window) override;
		virtual void Shutdown() override;

		virtual GfxResult CreateBuffer(Buffer* buffer, BufferDesc* desc, const void* initial_data) override;
		virtual GfxResult CreatePipeline(Pipeline* pipeline, PipelineDesc* desc) override;
		virtual GfxResult CreateTexture(Texture* texture, std::string path, bool bGenerateMips) override;
		virtual GfxResult CreateTexture(Texture* texture, TextureDesc* desc) override;
		virtual GfxResult CreateRenderPass(RenderPass* renderPass, RenderPassDesc* desc) override;
		virtual GfxResult CreateGPUTimer(GPUTimer* timer) override;

		virtual void SetName(Resource* child, std::string& name) override;

		virtual void BeginGPUTimer(GPUTimer* timer) override;
		virtual void EndGPUTimer(GPUTimer* timer) override;

		virtual GfxResult UpdateBufferData(Buffer* buffer, const void* data, uint32_t count = 0) override;

		virtual void GenerateMips(Texture* texture) override;

		virtual void ChangeResourceState(Texture* resource, ResourceState currentState, ResourceState desiredState, int subresource = -1) override;
		virtual void EnsureMsgResourceState(Texture* resource, ResourceState destResourceState) override;

		virtual uint64_t GetTextureID(Texture* texture) override;

		virtual GfxResult ReloadPipeline(Pipeline* pipeline) override;

		virtual void EnableImGui() override;
		virtual void ImGuiNewFrame() override;

		virtual void BindPipeline(Pipeline* pipeline) override;
		virtual void BindVertexBuffer(Buffer* vertexBuffer) override;
		virtual void BindIndexBuffer(Buffer* indexBuffer) override;
		virtual void BindParameter(const std::string& parameterName, Buffer* buffer) override;
		virtual void BindParameter(const std::string& parameterName, Texture* texture, TextureUsage usage = kReadOnly) override;
		virtual void BindParameter(const std::string& parameterName, void* data, size_t size) override;

		virtual void BeginRender() override;
		virtual void Render() override;
		virtual void EndRender() override;

		virtual void BeginRenderPass(RenderPass* renderPass) override;
		virtual void SetSwapchainTarget(RenderPass* renderPass) override;
		virtual void EndRenderPass(RenderPass* renderPass) override;

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t startVertexLocation = 0, uint32_t startInstanceLocation = 0) override;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t startIndexLocation = 0, uint32_t baseVertexLocation = 0, uint32_t startInstanceLocation = 0) override;
		virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

		virtual GfxResult Resize(uint32_t width, uint32_t height) override;

		virtual void ReadPixelFromTexture(uint32_t x, uint32_t y, Texture* texture, glm::vec4& pixel) override;

	private:
		bool IsGPUSuitable(VkPhysicalDevice& gpu);
		void AcquireQueueFamilyIndicesFamily();
		void CreateInstance();
		void CreateDevice();
	};
}
