#pragma once

#include <glm/glm.hpp>

#include "Core/Window.h"

#include "RHIDefinitions.h"
#include "RHIResources.h"
 
namespace Eden
{
	class DynamicRHI
	{
	protected:
		bool m_VSyncEnabled = false;
		bool m_bIsImguiInitialized = false;
		int32_t m_FrameIndex;
		API m_CurrentAPI;

	public:
		DynamicRHI() = default;
		virtual ~DynamicRHI()
		{
		}

		int32_t GetCurrentFrameIndex() { return m_FrameIndex; }
		API GetCurrentAPI() { return m_CurrentAPI; }

		virtual GfxResult Init(Window* window) = 0;
		virtual void Shutdown() = 0;

		virtual BufferRef     CreateBuffer(BufferDesc* desc, const void* initial_data) = 0;
		virtual PipelineRef   CreatePipeline(PipelineDesc* desc) = 0;
		virtual TextureRef    CreateTexture(std::string path, bool bGenerateMips) = 0;
		virtual TextureRef    CreateTexture(TextureDesc* desc) = 0;
		virtual RenderPassRef CreateRenderPass(RenderPassDesc* desc) = 0;
		virtual GPUTimerRef   CreateGPUTimer() = 0;

		virtual void BeginGPUTimer(GPUTimerRef timer) = 0;
		virtual void EndGPUTimer(GPUTimerRef timer) = 0;

		virtual GfxResult UpdateBufferData(BufferRef buffer, const void* data, uint32_t count = 0) = 0;
		virtual void GenerateMips(TextureRef texture) = 0;

		virtual void ChangeResourceState(TextureRef resource, ResourceState currentState, ResourceState desiredState, int subresource = -1) = 0;
		virtual void EnsureMsgResourceState(TextureRef resource, ResourceState destResourceState) = 0;

		virtual uint64_t GetTextureID(TextureRef texture) = 0;

		virtual GfxResult ReloadPipeline(PipelineRef pipeline) = 0;

		virtual void EnableImGui() = 0;
		virtual void ImGuiNewFrame() = 0;

		virtual void BindPipeline(PipelineRef pipeline) = 0;
		virtual void BindVertexBuffer(BufferRef vertexBuffer) = 0;
		virtual void BindIndexBuffer(BufferRef indexBuffer) = 0;
		virtual void BindParameter(const std::string& parameterName, BufferRef buffer) = 0;
		virtual void BindParameter(const std::string& parameterName, TextureRef texture, TextureUsage usage = kReadOnly) = 0;
		virtual void BindParameter(const std::string& parameterName, void* data, size_t size) = 0; // Use only for constants

		virtual void BeginRender() = 0;
		virtual void BeginRenderPass(RenderPassRef renderPass) = 0;
		virtual void SetSwapchainTarget(RenderPassRef renderPass) = 0;
		virtual void EndRenderPass(RenderPassRef renderPass) = 0;
		virtual void EndRender() = 0;

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t startVertexLocation = 0, uint32_t startInstanceLocation = 0) = 0;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t startIndexLocation = 0, uint32_t baseVertexLocation = 0, uint32_t startInstanceLocation = 0) = 0;
		virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;

		virtual void Render() = 0;
		virtual GfxResult Resize(uint32_t width, uint32_t height) = 0;

		virtual void ReadPixelFromTexture(uint32_t x, uint32_t y, TextureRef texture, glm::vec4& pixel) = 0;

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

