#pragma once

#include "Core/Base.h"
#include "Core/Assertions.h"
#include "Core/Window.h"
#include "RHI/RHI.h"
#include "d3dx12.h"
#include "D3D12Helper.h"

#include <d3d12.h>
#include <d3d12shader.h>
#include <dxc/dxcapi.h>
#include <wrl/client.h>
#include <filesystem>

#include <D3D12MemoryAllocator/D3D12MemAlloc.h>


using namespace Microsoft::WRL;


namespace Eden
{
	struct D3D12Resource : public ResourceInternal
	{
		ComPtr<ID3D12Resource> resource;
		ComPtr<D3D12MA::Allocation> allocation;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	};

	struct D3D12Buffer : public D3D12Resource
	{
		//! Created in case we need things for the buffer
	};

	struct D3D12Texture : public D3D12Resource
	{
		ComPtr<ID3D12Resource> uploadHeap;
		ComPtr<D3D12MA::Allocation> uploadAllocation;
		std::vector<D3D12_SUBRESOURCE_DATA> data;
	};

	struct D3D12Pipeline : public ResourceInternal
	{
		ComPtr<ID3D12RootSignature> rootSignature;
		ComPtr<ID3D12PipelineState> pipelineState;
		ComPtr<ID3D12ShaderReflection> pixel_reflection;
		ComPtr<ID3D12ShaderReflection> vertexReflection;
		ComPtr<ID3D12ShaderReflection> computeReflection;
	};

	struct DescriptorHeap
	{
		ComPtr<ID3D12DescriptorHeap> heap;
		uint32_t offset = 0;
	};

	struct D3D12RenderPass : public ResourceInternal
	{
		DescriptorHeap rtvHeap;
		DescriptorHeap dsvHeap;
	};

	struct D3D12GPUTimer : public ResourceInternal
	{
		ComPtr<ID3D12QueryHeap> queryHeap;
	};

	class D3D12RHI final : public IRHI
	{
		DWORD m_MessageCallbackCookie;
		ComPtr<ID3D12InfoQueue> m_InfoQueue;
		ComPtr<ID3D12Device> m_Device;
		ComPtr<IDXGIFactory4> m_Factory;
		ComPtr<IDXGIAdapter1> m_Adapter;
		ComPtr<IDXGISwapChain3> m_Swapchain;
		DescriptorHeap m_SRVHeap;
		ComPtr<ID3D12Fence> m_Fence;
		ComPtr<ID3D12CommandQueue> m_CommandQueue;
		ComPtr<ID3D12CommandAllocator> m_CommandAllocator[s_FrameCount];
		ComPtr<ID3D12GraphicsCommandList4> m_CommandList;
		CD3DX12_RECT m_Scissor;

		ComPtr<D3D12MA::Allocator> m_Allocator;

		// Buffer used to get the pixel from the "ObjectPicker" color attachment
		D3D12Resource pixelReadStagingBuffer;

		bool m_bIsImguiInitialized = false;

		ComPtr<IDxcUtils> m_DxcUtils;
		ComPtr<IDxcCompiler3> m_DxcCompiler;
		ComPtr<IDxcIncludeHandler> m_DxcIncludeHandler;

		HANDLE m_FenceEvent;
		uint64_t m_FenceValues[s_FrameCount] = { 0, 0 };

		D3D12_VERTEX_BUFFER_VIEW m_BoundVertexBuffer;
		D3D12_INDEX_BUFFER_VIEW m_BoundIndexBuffer;
		Pipeline* m_BoundPipeline;

		struct ShaderResult
		{
			ComPtr<IDxcBlob> blob;
			ComPtr<ID3D12ShaderReflection> reflection;
		};

		RenderPass* m_SwapchainTarget;

		// temp until I found another solution
		Pipeline m_MipsPipeline;

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
		virtual void BindParameter(const std::string& parameterName, void* data, size_t size) override; // Use only for constants

		virtual void BeginRender() override;
		virtual void BeginRenderPass(RenderPass* renderPass) override;
		virtual void SetSwapchainTarget(RenderPass* renderPass) override;
		virtual void EndRenderPass(RenderPass* renderPass) override;
		virtual void EndRender() override;

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t startVertexLocation = 0, uint32_t startInstanceLocation = 0) override;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t startIndexLocation = 0, uint32_t baseVertexLocation = 0, uint32_t startInstanceLocation = 0) override;
		virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

		virtual void Render() override;
		virtual GfxResult Resize(uint32_t width, uint32_t height) override;

		virtual void ReadPixelFromTexture(uint32_t x, uint32_t y, Texture* texture, glm::vec4& pixel) override;

	private:
		void PrepareDraw();
		void GetHardwareAdapter();
		void WaitForGPU();
		void CreateAttachments(RenderPass* renderPass);
		uint32_t GetRootParameterIndex(const std::string& parameterName);
		void CreateRootSignature(Pipeline* pipeline);
		ShaderResult CompileShader(std::filesystem::path filePath, ShaderStage stage);
		D3D12_STATIC_SAMPLER_DESC CreateSamplerDesc(uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility, D3D12_TEXTURE_ADDRESS_MODE addressMode);

		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(DescriptorHeap* descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, int32_t offset = 0);
		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(DescriptorHeap* descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, int32_t offset = 0);
		uint32_t AllocateHandle(DescriptorHeap* descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		void CreateGraphicsPipeline(Pipeline* pipeline, PipelineDesc* desc);
		void CreateComputePipeline(Pipeline* pipeline, PipelineDesc* desc);

		void CreateCommands();

		void BindRootParameter(const std::string& parameterName, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

		void SetTextureData(Texture* texture);

		GfxResult CreateDescriptorHeap(DescriptorHeap* descriptorHeap, uint32_t num_descriptors, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
		void SetDescriptorHeap(DescriptorHeap* descriptorHeap);
	};

	namespace Helpers
	{
		constexpr DXGI_FORMAT ConvertFormat(Format value)
		{
			switch (value)
			{
			case Format::kUnknown:
				return DXGI_FORMAT_UNKNOWN;
			case Format::kR8_UNORM:
				return DXGI_FORMAT_R8_UNORM;
			case Format::kR8_UINT:
				return DXGI_FORMAT_R8_UINT;
			case Format::kR16_UINT:
				return DXGI_FORMAT_R16_UINT;
			case Format::kR32_UINT:
				return DXGI_FORMAT_R32_UINT;
			case Format::kR32_FLOAT:
				return DXGI_FORMAT_R32_FLOAT;
			case Format::kRG8_UNORM:
				return DXGI_FORMAT_R8G8_UNORM;
			case Format::kRG16_FLOAT:
				return DXGI_FORMAT_R16G16_FLOAT;
			case Format::kRG32_FLOAT:
				return DXGI_FORMAT_R32G32_FLOAT;
			case Format::kRGBA8_UNORM:
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			case Format::kRGBA16_FLOAT:
				return DXGI_FORMAT_R16G16B16A16_FLOAT;
			case Format::kRGBA32_FLOAT:
				return DXGI_FORMAT_R32G32B32A32_FLOAT;
			case Format::kSRGB:
				return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

			// Depth Formats
			case Format::kDepth32FloatStencil8X24_UINT:
				return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
			case Format::kDepth32_FLOAT:
				return DXGI_FORMAT_D32_FLOAT;
			case Format::kDepth24Stencil8:
				return DXGI_FORMAT_D24_UNORM_S8_UINT;

			// Typeless Formats
			case Format::kR32_FLOAT_X8X24_TYPELESS:
				return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
			case Format::kR32_TYPELESS:
				return DXGI_FORMAT_R32_TYPELESS;
			case Format::kR24_X8_TYPELESS:
				return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			default:
				return DXGI_FORMAT_UNKNOWN;
			}
		}

		constexpr D3D12_BLEND ConvertBlend(Blend blend)
		{
			switch (blend)
			{
			case Blend::kZero:
				return D3D12_BLEND_ZERO;
			case Blend::kOne:
				return D3D12_BLEND_ONE;
			case Blend::kSrcColor:
				return D3D12_BLEND_SRC_COLOR;
			case Blend::kInvSrcColor:
				return D3D12_BLEND_INV_SRC_COLOR;
			case Blend::kSrcAlpha:
				return D3D12_BLEND_SRC_ALPHA;
			case Blend::kInvSrcAlpha:
				return D3D12_BLEND_INV_SRC_ALPHA;
			case Blend::kDestAlpha:
				return D3D12_BLEND_DEST_ALPHA;
			case Blend::kInvDestAlpha:
				return D3D12_BLEND_INV_DEST_ALPHA;
			case Blend::kDestColor:
				return D3D12_BLEND_DEST_COLOR;
			case Blend::kInvDestColor:
				return D3D12_BLEND_INV_DEST_COLOR;
			case Blend::kSrcAlphaSat:
				return D3D12_BLEND_SRC_ALPHA_SAT;
			case Blend::kBlendFactor:
				return D3D12_BLEND_BLEND_FACTOR;
			case Blend::kInvBlendFactor:
				return D3D12_BLEND_INV_BLEND_FACTOR;
			case Blend::kSrc1Color:
				return D3D12_BLEND_SRC1_COLOR;
			case Blend::kInvSrc1Color:
				return D3D12_BLEND_INV_SRC1_COLOR;
			case Blend::kSrc1Alpha:
				return D3D12_BLEND_SRC1_ALPHA;
			case Blend::kInvSrc1Alpha:
				return D3D12_BLEND_INV_SRC1_ALPHA;
			default:
				return D3D12_BLEND_ZERO;
			}
		}

		constexpr D3D12_BLEND_OP ConvertBlendOp(BlendOp blendOp)
		{
			switch (blendOp)
			{
			case BlendOp::kAdd:
				return D3D12_BLEND_OP_ADD;
			case BlendOp::kSubtract:
				return D3D12_BLEND_OP_SUBTRACT;
			case BlendOp::kRevSubtract:
				return D3D12_BLEND_OP_REV_SUBTRACT;
			case BlendOp::kMin:
				return D3D12_BLEND_OP_MIN;
			case BlendOp::kMax:
				return D3D12_BLEND_OP_MAX;
			default:
				return D3D12_BLEND_OP_ADD;
			}
		}

		constexpr D3D12_COMPARISON_FUNC ConvertComparisonFunc(ComparisonFunc compFunc)
		{
			switch (compFunc)
			{
			case ComparisonFunc::kNever:
				return D3D12_COMPARISON_FUNC_NEVER;
			case ComparisonFunc::kLess:
				return D3D12_COMPARISON_FUNC_LESS;
			case ComparisonFunc::kEqual:
				return D3D12_COMPARISON_FUNC_EQUAL;
			case ComparisonFunc::kLessEqual:
				return D3D12_COMPARISON_FUNC_LESS_EQUAL;
			case ComparisonFunc::kGreater:
				return D3D12_COMPARISON_FUNC_GREATER;
			case ComparisonFunc::kNotEqual:
				return D3D12_COMPARISON_FUNC_NOT_EQUAL;
			case ComparisonFunc::kGreaterEqual:
				return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
			case ComparisonFunc::kAlways:
				return D3D12_COMPARISON_FUNC_ALWAYS;
			default:
				return D3D12_COMPARISON_FUNC_NEVER;
			}
		}

		constexpr D3D12_CULL_MODE ConvertCullMode(CullMode cull_mode)
		{
			switch (cull_mode)
			{
			case CullMode::kNone:
				return D3D12_CULL_MODE_NONE;
			case Eden::CullMode::kFront:
				return D3D12_CULL_MODE_FRONT;
			case Eden::CullMode::kBack:
				return D3D12_CULL_MODE_BACK;
			default:
				return D3D12_CULL_MODE_NONE;
			}
		}

		constexpr D3D12_RESOURCE_STATES ConvertResourceState(ResourceState state)
		{
			switch (state)
			{
			case ResourceState::kCommon:
				return D3D12_RESOURCE_STATE_COMMON;
			case ResourceState::kRenderTarget:
				return D3D12_RESOURCE_STATE_RENDER_TARGET;
			case ResourceState::kPresent:
				return D3D12_RESOURCE_STATE_PRESENT;
			case ResourceState::kUnorderedAccess:
				return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			case ResourceState::kPixelShader:
				return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			case ResourceState::kNonPixelShader:
				return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			case ResourceState::kRead:
				return D3D12_RESOURCE_STATE_GENERIC_READ;
			case ResourceState::kCopyDest:
				return D3D12_RESOURCE_STATE_COPY_DEST;
			case ResourceState::kCopySrc:
				return D3D12_RESOURCE_STATE_COPY_SOURCE;
			case ResourceState::kDepthRead:
				return D3D12_RESOURCE_STATE_DEPTH_READ;
			case ResourceState::kDepthWrite:
				return D3D12_RESOURCE_STATE_DEPTH_WRITE;
			default:
				return D3D12_RESOURCE_STATE_COMMON;
			}
		}
	}
}

