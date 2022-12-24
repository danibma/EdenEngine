#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>

#include <glm/glm.hpp>

#include "Core/Window.h"
 
namespace Eden
{
	constexpr uint32_t s_FrameCount = 2;
	// TODO: Right now just use a huge number here, and refactor when bindless descriptors are added
	constexpr uint32_t s_SRVDescriptorCount = 10000; 

	enum ShaderStage
	{
		VS = 0,
		PS,
		CS,
		COUNT // Don't use this
	};

	enum class ComparisonFunc
	{
		NEVER,
		LESS,
		EQUAL,
		LESS_EQUAL,
		GREATER,
		NOT_EQUAL,
		GREATER_EQUAL,
		ALWAYS
	};

	enum class CullMode
	{
		NONE,
		FRONT,
		BACK
	};

	enum class Blend
	{
		ZERO,
		ONE,
		SRC_COLOR,
		INV_SRC_COLOR,
		SRC_ALPHA,
		INV_SRC_ALPHA,
		DEST_ALPHA,
		INV_DEST_ALPHA,
		DEST_COLOR,
		INV_DEST_COLOR,
		SRC_ALPHA_SAT,
		BLEND_FACTOR,
		INV_BLEND_FACTOR,
		SRC1_COLOR,
		INV_SRC1_COLOR,
		SRC1_ALPHA,
		INV_SRC1_ALPHA,
	};
	enum class BlendOp
	{
		ADD,
		SUBTRACT,
		REV_SUBTRACT,
		MIN,
		MAX,
	};

	enum class Format
	{
		UNKNOWN,

		R8_UNORM,
		R8_UINT,
		R16_UINT,
		R32_UINT,
		R32_FLOAT,
		RG8_UNORM,
		RG16_FLOAT,
		RG32_FLOAT,
		RGBA8_UNORM,
		RGBA16_FLOAT,
		RGBA32_FLOAT,

		SRGB,

		DEPTH32_FLOAT_STENCIL8X24_UINT,
		DEPTH32_FLOAT,
		DEPTH24_STENCIL8,

		R32_FLOAT_X8X24_TYPELESS,
		R32_TYPELESS,
		R24_X8_TYPELESS,

		// Defaults
		Depth = DEPTH24_STENCIL8,
	};

	enum class ResourceState
	{
		COMMON,
		RENDER_TARGET,
		PRESENT,
		UNORDERED_ACCESS,
		PIXEL_SHADER,
		NON_PIXEL_SHADER,
		READ,
		COPY_DEST,
		COPY_SRC,
		DEPTH_READ,
		DEPTH_WRITE
	};

	enum PipelineType
	{
		Graphics,
		Compute // Not implemented
	};

	enum API
	{
		D3D12
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
		bool IsValid() { return internal_state.get() != nullptr; }
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
		std::vector<std::shared_ptr<Texture>> colorAttachments;
		std::shared_ptr<Texture> depthStencil;
		RenderPassDesc desc;
	};

	struct PipelineDesc
	{
		std::string programName;
		bool bEnableBlending = false;
		bool bIsFrontCounterClockwise = true;
		float minDepth = 0.0f;
		CullMode cull_mode = CullMode::BACK;
		ComparisonFunc depthFunc = ComparisonFunc::LESS;
		PipelineType type = Graphics;
		std::shared_ptr<RenderPass> renderPass;
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

		std::shared_ptr<Buffer> readbackBuffer;
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

		virtual void Init(Window* window) = 0;
		virtual void Shutdown() = 0;

		virtual std::shared_ptr<Buffer> CreateBuffer(BufferDesc* desc, const void* initial_data) = 0;
		virtual std::shared_ptr<Pipeline> CreatePipeline(PipelineDesc* desc) = 0;
		virtual std::shared_ptr<Texture> CreateTexture(std::string path, bool bGenerateMips) = 0;
		virtual std::shared_ptr<Texture> CreateTexture(TextureDesc* desc) = 0;
		virtual std::shared_ptr<RenderPass> CreateRenderPass(RenderPassDesc* desc) = 0;
		virtual std::shared_ptr<GPUTimer> CreateGPUTimer() = 0;

		virtual void SetName(std::shared_ptr<Resource> child, std::string& name) = 0;

		virtual void BeginGPUTimer(std::shared_ptr<GPUTimer>& timer) = 0;
		virtual void EndGPUTimer(std::shared_ptr<GPUTimer>& timer) = 0;

		virtual void UpdateBufferData(std::shared_ptr<Buffer>& buffer, const void* data, uint32_t count = 0) = 0;
		virtual void GenerateMips(std::shared_ptr<Texture>& texture) = 0;

		virtual void ChangeResourceState(std::shared_ptr<Texture>& resource, ResourceState currentState, ResourceState desiredState, int subresource = -1) = 0;
		virtual void ensureMsgResourceState(std::shared_ptr<Texture>& resource, ResourceState destResourceState) = 0;

		virtual uint64_t GetTextureID(std::shared_ptr<Texture>& texture) = 0;

		virtual void ReloadPipeline(std::shared_ptr<Pipeline>& pipeline) = 0;

		virtual void EnableImGui() = 0;
		virtual void ImGuiNewFrame() = 0;

		virtual void BindPipeline(const std::shared_ptr<Pipeline>& pipeline) = 0;
		virtual void BindVertexBuffer(std::shared_ptr<Buffer>& vertexBuffer) = 0;
		virtual void BindIndexBuffer(std::shared_ptr<Buffer>& indexBuffer) = 0;
		virtual void BindParameter(const std::string& parameterName, std::shared_ptr<Buffer>& buffer) = 0;
		virtual void BindParameter(const std::string& parameterName, std::shared_ptr<Texture>& texture, TextureUsage usage = kReadOnly) = 0;
		virtual void BindParameter(const std::string& parameterName, void* data, size_t size) = 0; // Use only for constants

		virtual void BeginRender() = 0;
		virtual void BeginRenderPass(std::shared_ptr<RenderPass>& renderPass) = 0;
		virtual void SetSwapchainTarget(std::shared_ptr<RenderPass>& renderPass) = 0;
		virtual void EndRenderPass(std::shared_ptr<RenderPass>& renderPass) = 0;
		virtual void EndRender() = 0;

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t startVertexLocation = 0, uint32_t startInstanceLocation = 0) = 0;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t startIndexLocation = 0, uint32_t baseVertexLocation = 0, uint32_t startInstanceLocation = 0) = 0;
		virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;

		virtual void Render() = 0;
		virtual void Resize(uint32_t width, uint32_t height) = 0;

		virtual void ReadPixelFromTexture(uint32_t x, uint32_t y, std::shared_ptr<Texture> texture, glm::vec4& pixel) = 0;

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
			if (format == Format::DEPTH32_FLOAT_STENCIL8X24_UINT ||
				format == Format::DEPTH32_FLOAT ||
				format == Format::DEPTH24_STENCIL8)
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
			case VS:
				return "Vertex";
			case PS:
				return "Pixel";
			default:
				return "Null";
			}
		}

		inline const char* APIToString(const API api)
		{
			switch (api)
			{
			case API::D3D12:
				return "<D3D12>";
			default:
				return "";
			}
		}
	}
}

