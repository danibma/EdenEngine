#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Graphics/RHI.h"

namespace Eden
{
	class VulkanRHI : public IRHI
	{
		bool m_ImguiInitialized = false;

		VkInstance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessenger;
		VkPhysicalDevice m_GPU;
		VkDevice m_Device;
		VkSurfaceKHR m_Surface;
		VkSwapchainKHR m_Swapchain;
		VkFormat m_SwapchainImageFormat;
		std::vector<VkImage> m_SwapchainImages;
		std::vector<VkImageView> m_SwapchainImageViews;
		VkQueue m_GraphicsQueue;
		uint32_t m_GraphicsQueueFamily;
		VkCommandPool m_CommandPool;
		VkCommandBuffer m_CommandBuffer;
		VkSemaphore m_PresentSemaphore, m_RenderSemaphore;
		VkFence m_RenderFence;

		VkRenderPass m_RenderPass;
		std::vector<VkFramebuffer> m_FrameBuffers;

	public:
		void Init(Window* window) override;
		void Shutdown() override;

		std::shared_ptr<Buffer> CreateBuffer(BufferDesc* desc, const void* initial_data) override;
		std::shared_ptr<Pipeline> CreatePipeline(PipelineDesc* desc) override;
		std::shared_ptr<Texture> CreateTexture(std::string path) override;
		std::shared_ptr<Texture> CreateTexture(TextureDesc* desc) override;
		std::shared_ptr<RenderPass> CreateRenderPass(RenderPassDesc* desc) override;

		void UpdateBufferData(std::shared_ptr<Buffer>& buffer, const void* data, uint32_t count = 0) override;

		uint64_t GetTextureID(std::shared_ptr<Texture>& texture) override;

		void ReloadPipeline(std::shared_ptr<Pipeline>& pipeline) override;

		void EnableImGui() override;
		void ImGuiNewFrame() override;

		void BindPipeline(const std::shared_ptr<Pipeline>& pipeline) override;
		void BindVertexBuffer(std::shared_ptr<Buffer>& vertex_buffer) override;
		void BindIndexBuffer(std::shared_ptr<Buffer>& index_buffer) override;
		void BindParameter(const std::string& parameter_name, const std::shared_ptr<Buffer>& buffer) override;
		void BindParameter(const std::string& parameter_name, const std::shared_ptr<Texture>& texture) override;

		void BeginRender() override;
		void BeginRenderPass(std::shared_ptr<RenderPass>& render_pass) override;
		void SetRTRenderPass(std::shared_ptr<RenderPass>& render_pass) override;
		void EndRenderPass(std::shared_ptr<RenderPass>& render_pass) override;
		void EndRender() override;

		void Draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t start_vertex_location = 0, uint32_t start_instance_location = 0) override;
		void DrawIndexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t start_index_location = 0, uint32_t base_vertex_location = 0, uint32_t start_instance_location = 0) override;

		void Render() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ResizeRenderPassTexture(std::shared_ptr<RenderPass>& render_pass, uint32_t width, uint32_t height) override;

	private:
		void WaitGPU();
	};
}

