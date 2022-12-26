#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>

#include <glm/glm.hpp>

#include "Core/Window.h"
#include "Core/Memory/Memory.h"
 
namespace Eden
{
	constexpr uint32_t s_FrameCount = 2;
	// TODO: Right now just use a huge number here, and refactor when bindless descriptors are added
	constexpr uint32_t s_SRVDescriptorCount = 10000; 

	enum class GfxResult
	{
		kNoError = 0,
		kInvalidParameter,
		kOutOfMemory,
		kInternalError,

		kCount
	};

	enum class ShaderStage
	{
		kVS = 0,
		kPS,
		kCS,
		kCount // Don't use this
	};

	enum class ComparisonFunc
	{
		kNever,
		kLess,
		kEqual,
		kLessEqual,
		kGreater,
		kNotEqual,
		kGreaterEqual,
		kAlways
	};

	enum class CullMode
	{
		kNone,
		kFront,
		kBack
	};

	enum class Blend
	{
		kZero,
		kOne,
		kSrcColor,
		kInvSrcColor,
		kSrcAlpha,
		kInvSrcAlpha,
		kDestAlpha,
		kInvDestAlpha,
		kDestColor,
		kInvDestColor,
		kSrcAlphaSat,
		kBlendFactor,
		kInvBlendFactor,
		kSrc1Color,
		kInvSrc1Color,
		kSrc1Alpha,
		kInvSrc1Alpha,
	};
	enum class BlendOp
	{
		kAdd,
		kSubtract,
		kRevSubtract,
		kMin,
		kMax,
	};

	enum class Format
	{
		kUnknown,

		kR8_UNORM,
		kR8_UINT,
		kR16_UINT,
		kR32_UINT,
		kR32_FLOAT,
		kRG8_UNORM,
		kRG16_FLOAT,
		kRG32_FLOAT,
		kRGBA8_UNORM,
		kRGBA16_FLOAT,
		kRGBA32_FLOAT,

		kSRGB,

		kDepth32FloatStencil8X24_UINT,
		kDepth32_FLOAT,
		kDepth24Stencil8,

		kR32_FLOAT_X8X24_TYPELESS,
		kR32_TYPELESS,
		kR24_X8_TYPELESS,

		// Defaults
		Depth = kDepth24Stencil8,
	};

	enum class ResourceState
	{
		kCommon,
		kRenderTarget,
		kPresent,
		kUnorderedAccess,
		kPixelShader,
		kNonPixelShader,
		kRead,
		kCopyDest,
		kCopySrc,
		kDepthRead,
		kDepthWrite
	};

	enum PipelineType
	{
		kPipelineType_Graphics,
		kPipelineType_Compute // Not implemented
	};

	enum API
	{
		kApi_D3D12
	};

	enum TextureUsage
	{
		kReadOnly,
		kReadWrite // Used for UAV's
	};

	struct GraphicsChild
	{
		std::string debugName;
		std::shared_ptr<void> internal_state;
		bool IsValid() { return internal_state != nullptr; }
		operator bool()
		{
			return IsValid();
		}
	};

	struct Resource : GraphicsChild
	{
		bool bIsInitialized = false;
		ResourceState currentState;
	};

	struct BufferDesc
	{
		enum BufferUsage
		{
			Vertex_Index,
			Uniform, // ConstantBuffer in D3D12
			Storage, // Structured, UAV, RWBuffer in D3D12
			Readback
		};

		uint32_t stride;
		uint32_t elementCount;
		BufferUsage usage = Uniform;
	};

	struct Buffer : public Resource 
	{
		void* mappedData;
		uint32_t size;
		BufferDesc desc;
	};

	struct TextureDesc
	{
		enum TextureType
		{
			Texture2D,
			Texture3D,  // Not implemented
			TextureCube	// Not implemented
		};

		void* data;
		uint32_t width;
		uint32_t height;
		bool bIsSrgb = false;
		bool bIsStorage = false;
		bool bGenerateMips = true;
		TextureType type = Texture2D;
	};

	struct Texture: public Resource
	{
		Format imageFormat;
		uint32_t mipCount;
		TextureDesc desc;
	};

	struct RenderPassDesc
	{
		std::string debugName;
		bool bIsImguiPass = false;
		bool bIsSwapchainTarget = false;
		std::vector<Format> attachmentsFormats;
		uint32_t width = 0, height = 0;
		glm::vec4 clearColor = glm::vec4(0, 0, 0, 1);
	};

	struct RenderPass : GraphicsChild
	{
		std::vector<Texture> colorAttachments;
		Texture depthStencil;
		RenderPassDesc desc;
	};

	struct PipelineDesc
	{
		std::string programName;
		bool bEnableBlending = false;
		bool bIsFrontCounterClockwise = true;
		float minDepth = 0.0f;
		CullMode cull_mode = CullMode::kBack;
		ComparisonFunc depthFunc = ComparisonFunc::kLess;
		PipelineType type = kPipelineType_Graphics;
		RenderPass* renderPass;
	};

	struct Pipeline : GraphicsChild
	{
		PipelineDesc desc;

		// Shader Reflection data
		// name, rootParameterIndex
		std::unordered_map<std::string, uint32_t> rootParameterIndices;
	};

	struct GPUTimer : GraphicsChild
	{
		double elapsedTime = 0.0f;

		Buffer readbackBuffer;
	};

	class IRHI
	{
	protected:
		bool m_bIsImguiInitialized = false;
		int32_t m_FrameIndex;
		API m_CurrentAPI;

	public:
		IRHI() = default;

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
				return "<D3D12>";
			default:
				return "";
			}
		}
	}
}

