#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>

#include "Core/Window.h"

namespace Eden
{
	constexpr uint32_t s_FrameCount = 2;

	enum ShaderStage
	{
		Vertex = 0,
		Pixel
	};

	enum class ComparisonFunc
	{
		NEVER,
		LESS,
		EQUAL,
		LESS_EQUAL,
		GRAETER,
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

		DEPTH32FSTENCIL8_UINT,
		DEPTH32_FLOAT,
		DEPTH24STENCIL8,

		// Defaults
		Depth = DEPTH24STENCIL8,
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

	struct GraphicsChild
	{
		std::shared_ptr<void> internal_state;
		bool IsValid() { return internal_state.get() != nullptr; }
		operator bool()
		{
			return IsValid();
		}
	};

	struct Resource : GraphicsChild
	{
		bool initialized = false;
	};

	struct BufferDesc
	{
		enum BufferUsage
		{
			Vertex_Index,
			Uniform, // ConstantBuffer in D3D12
			Storage // Structured, UAV, RWBuffer in D3D12
		};

		uint32_t stride;
		uint32_t element_count;
		BufferUsage usage = Uniform;
	};

	struct Buffer : public Resource 
	{
		void* mapped_data;
		uint32_t size;
		BufferDesc desc;
	};

	struct TextureDesc
	{
		enum TextureUsage
		{
			Texture2D,
			Texture3D,  // Not implemented
			TextureCube	// Not implemented
		};

		void* data;
		uint32_t width;
		uint32_t height;
		bool srgb = false;
		TextureUsage usage = Texture2D;
	};

	struct Texture: public Resource
	{
		Format image_format;
		TextureDesc desc;
	};

	struct RenderPassDesc
	{
		std::string debug_name;
		bool imgui_pass = false;
		bool swapchain_target = false;
		std::vector<Format> attachments_formats;
	};

	struct RenderPass : GraphicsChild
	{
		uint32_t width, height;
		std::vector<std::shared_ptr<Texture>> color_attachments;
		std::shared_ptr<Texture> depth_stencil;
		RenderPassDesc desc;
	};

	struct PipelineDesc
	{
		std::string program_name;
		bool enable_blending = false;
		bool front_counter_clockwise = true;
		float min_depth = 0.0f;
		CullMode cull_mode = CullMode::BACK;
		ComparisonFunc depth_func = ComparisonFunc::LESS;
		PipelineType type = Graphics;
		std::shared_ptr<RenderPass> render_pass;
	};

	struct Pipeline : GraphicsChild
	{
		std::string debug_name;
		PipelineDesc desc;

		// Shader Reflection data
		// name, rootParameterIndex
		std::unordered_map<std::string, uint32_t> root_parameter_indices;
	};

	class IRHI
	{
	protected:
		bool m_ImguiInitialized = false;
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
		virtual std::shared_ptr<Texture> CreateTexture(std::string path) = 0;
		virtual std::shared_ptr<Texture> CreateTexture(TextureDesc* desc) = 0;
		virtual std::shared_ptr<RenderPass> CreateRenderPass(RenderPassDesc* desc) = 0;

		virtual void UpdateBufferData(std::shared_ptr<Buffer>& buffer, const void* data, uint32_t count = 0) = 0;

		virtual uint64_t GetTextureID(std::shared_ptr<Texture>& texture) = 0;

		virtual void ReloadPipeline(std::shared_ptr<Pipeline>& pipeline) = 0;

		virtual void EnableImGui() = 0;
		virtual void ImGuiNewFrame() = 0;

		virtual void BindPipeline(const std::shared_ptr<Pipeline>& pipeline) = 0;
		virtual void BindVertexBuffer(std::shared_ptr<Buffer>& vertex_buffer) = 0;
		virtual void BindIndexBuffer(std::shared_ptr<Buffer>& index_buffer) = 0;
		virtual void BindParameter(const std::string& parameter_name, const std::shared_ptr<Buffer>& buffer) = 0;
		virtual void BindParameter(const std::string& parameter_name, const std::shared_ptr<Texture>& texture) = 0;

		virtual void BeginRender() = 0;
		virtual void BeginRenderPass(std::shared_ptr<RenderPass>& render_pass) = 0;
		virtual void SetSwapchainTarget(std::shared_ptr<RenderPass>& render_pass) = 0;
		virtual void EndRenderPass(std::shared_ptr<RenderPass>& render_pass) = 0;
		virtual void EndRender() = 0;

		virtual void Draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t start_vertex_location = 0, uint32_t start_instance_location = 0) = 0;
		virtual void DrawIndexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t start_index_location = 0, uint32_t base_vertex_location = 0, uint32_t start_instance_location = 0) = 0;

		virtual void Render() = 0;
		virtual void Resize(uint32_t width, uint32_t height) = 0;

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
			if (format == Format::DEPTH32FSTENCIL8_UINT ||
				format == Format::DEPTH32_FLOAT ||
				format == Format::DEPTH24STENCIL8)
			{
				return true;
			}
			return false;
		}
	};

	namespace Utils
	{
		inline const char* ShaderStageToString(const ShaderStage target)
		{
			switch (target)
			{
			case Vertex:
				return "Vertex";
			case Pixel:
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

