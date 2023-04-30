#pragma once

#include <glm/glm.hpp>

#include "Core/Window.h"

#include "RHIDefinitions.h"
#include "RHIResources.h"
 
namespace Eden
{
	class IRHI
	{
	protected:
		bool m_VSyncEnabled = false;
		bool m_bIsImguiInitialized = false;
		int32_t m_FrameIndex;
		API m_CurrentAPI;

	public:
		IRHI() = default;
		virtual ~IRHI()
		{
		}

		int32_t GetCurrentFrameIndex() { return m_FrameIndex; }
		API GetCurrentAPI() { return m_CurrentAPI; }

		virtual GfxResult Init(Window* window) = 0;
		virtual void Shutdown() = 0;

		virtual GfxResult CreateBuffer(Buffer* buffer, BufferDesc* desc, const void* initial_data) = 0;
		virtual GfxResult CreatePipeline(Pipeline* pipeline, PipelineDesc* desc) = 0;
		virtual GfxResult CreateTexture(Texture* texture, std::string path, bool bGenerateMips) = 0;
		virtual GfxResult CreateTexture(Texture* texture, TextureDesc* desc) = 0;
		virtual GfxResult CreateRenderPass(RenderPass* renderPass, RenderPassDesc* desc) = 0;
		virtual GfxResult CreateGPUTimer(GPUTimer* timer) = 0;

		virtual void SetName(Resource* child, std::string& name) = 0;

		virtual void BeginGPUTimer(GPUTimer* timer) = 0;
		virtual void EndGPUTimer(GPUTimer* timer) = 0;

		virtual GfxResult UpdateBufferData(Buffer* buffer, const void* data, uint32_t count = 0) = 0;
		virtual void GenerateMips(Texture* texture) = 0;

		virtual void ChangeResourceState(Texture* resource, ResourceState currentState, ResourceState desiredState, int subresource = -1) = 0;
		virtual void EnsureMsgResourceState(Texture* resource, ResourceState destResourceState) = 0;

		virtual uint64_t GetTextureID(Texture* texture) = 0;

		virtual GfxResult ReloadPipeline(Pipeline* pipeline) = 0;

		virtual void EnableImGui() = 0;
		virtual void ImGuiNewFrame() = 0;

		virtual void BindPipeline(Pipeline* pipeline) = 0;
		virtual void BindVertexBuffer(Buffer* vertexBuffer) = 0;
		virtual void BindIndexBuffer(Buffer* indexBuffer) = 0;
		virtual void BindParameter(const std::string& parameterName, Buffer* buffer) = 0;
		virtual void BindParameter(const std::string& parameterName, Texture* texture, TextureUsage usage = kReadOnly) = 0;
		virtual void BindParameter(const std::string& parameterName, void* data, size_t size) = 0; // Use only for constants

		virtual void BeginRender() = 0;
		virtual void BeginRenderPass(RenderPass* renderPass) = 0;
		virtual void SetSwapchainTarget(RenderPass* renderPass) = 0;
		virtual void EndRenderPass(RenderPass* renderPass) = 0;
		virtual void EndRender() = 0;

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t startVertexLocation = 0, uint32_t startInstanceLocation = 0) = 0;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t startIndexLocation = 0, uint32_t baseVertexLocation = 0, uint32_t startInstanceLocation = 0) = 0;
		virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;

		virtual void Render() = 0;
		virtual GfxResult Resize(uint32_t width, uint32_t height) = 0;

		virtual void ReadPixelFromTexture(uint32_t x, uint32_t y, Texture* texture, glm::vec4& pixel) = 0;

	protected:
		int GetDepthFormatIndex(std::vector<Format>& formats)
		{
			for (int i = 0; i < formats.size(); ++i)
			{
				if (IsDepthFormat(formats[i]))
					return i;
			}

			return -1;
		}

		bool IsDepthFormat(Format format)
		{
			if (format == Format::kDepth32FloatStencil8X24_UINT ||
				format == Format::kDepth32_FLOAT ||
				format == Format::kDepth24Stencil8)
			{
				return true;
			}
			return false;
		}

		uint32_t CalculateMipCount(uint32_t width, uint32_t height)
		{
			return (uint32_t)std::floor(std::log2(glm::min<uint32_t>(width, height))) + 1;
		}
	};

	namespace Utils
	{
		inline const char* ShaderStageToString(const ShaderStage target)
		{
			switch (target)
			{
			case ShaderStage::kVS:
				return "Vertex";
			case ShaderStage::kPS:
				return "Pixel";
			default:
				return "Null";
			}
		}

		inline const char* APIToString(const API api)
		{
			switch (api)
			{
			case API::kApi_D3D12:
				return "D3D12";
			case API::kApi_Vulkan:
				return "Vulkan";
			default:
				return "";
			}
		}
	}
}

