#pragma once

#include "Core/Base.h"
#include "Core/Assertions.h"
#include "Core/Window.h"
#include "Graphics/RHI.h"
#include "d3dx12.h"

#include <d3d12.h>
#include <d3d12shader.h>
#include <dxc/dxcapi.h>
#include <wrl/client.h>
#include <filesystem>

#include <D3D12MemoryAllocator/D3D12MemAlloc.h>
#include <WinPixEventRuntime/pix3.h>


using namespace Microsoft::WRL;

// Helper macros
#define SAFE_RELEASE(x) if (x) x->Release();

namespace Eden
{
	struct D3D12Resource
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
		//! Created in case we need things specific for the texture
	};

	struct D3D12Pipeline
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

	struct D3D12RenderPass
	{
		std::shared_ptr<DescriptorHeap> rtvHeap;
		std::shared_ptr<DescriptorHeap> dsvHeap;
		D3D12_RENDER_PASS_RENDER_TARGET_DESC rtvDesc;
		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC dsvDesc;
	};

	struct D3D12GPUTimer
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
		std::shared_ptr<DescriptorHeap> m_SRVHeap; // TODO: Move this the descriptor heap things into the renderer, when the renderer is made
		ComPtr<ID3D12Fence> m_Fence;
		ComPtr<ID3D12CommandQueue> m_CommandQueue;
		ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
		ComPtr<ID3D12GraphicsCommandList4> m_CommandList;
		CD3DX12_RECT m_Scissor;

		ComPtr<D3D12MA::Allocator> m_Allocator;

		bool m_bIsImguiInitialized = false;

		ComPtr<IDxcUtils> m_DxcUtils;
		ComPtr<IDxcCompiler3> m_DxcCompiler;
		ComPtr<IDxcIncludeHandler> m_DxcIncludeHandler;

		HANDLE m_FenceEvent;
		uint64_t m_FenceValues[s_FrameCount] = { 0, 0 };

		D3D12_VERTEX_BUFFER_VIEW m_BoundVertexBuffer;
		D3D12_INDEX_BUFFER_VIEW m_BoundIndexBuffer;
		std::shared_ptr<Pipeline> m_BoundPipeline;

		struct ShaderResult
		{
			ComPtr<IDxcBlob> blob;
			ComPtr<ID3D12ShaderReflection> reflection;
		};

		std::shared_ptr<RenderPass> m_SwapchainTarget;

		// temp until I found another solution
		std::shared_ptr<Pipeline> m_MipsPipeline;

	public:
		virtual void Init(Window* window) override;
		virtual void Shutdown() override;

		virtual std::shared_ptr<Buffer> CreateBuffer(BufferDesc* desc, const void* data) override;
		virtual std::shared_ptr<Pipeline> CreatePipeline(PipelineDesc* desc) override;
		virtual std::shared_ptr<Texture> CreateTexture(std::string path, bool bGenerateMips) override;
		virtual std::shared_ptr<Texture> CreateTexture(TextureDesc* desc) override;
		virtual std::shared_ptr<RenderPass> CreateRenderPass(RenderPassDesc* desc) override;
		virtual std::shared_ptr<GPUTimer> CreateGPUTimer() override;

		virtual void SetName(std::shared_ptr<Resource> child, std::string& name) override;

		virtual void BeginGPUTimer(std::shared_ptr<GPUTimer>& timer) override;
		virtual void EndGPUTimer(std::shared_ptr<GPUTimer>& timer) override;

		virtual void UpdateBufferData(std::shared_ptr<Buffer>& buffer, const void* data, uint32_t count = 0) override;
		virtual void GenerateMips(std::shared_ptr<Texture>& texture) override;

		virtual void ChangeResourceState(std::shared_ptr<Texture>& resource, ResourceState currentState, ResourceState desiredState, int subresource = -1) override;
		virtual void EnsureResourceState(std::shared_ptr<Texture>& resource, ResourceState destResourceState) override;

		virtual uint64_t GetTextureID(std::shared_ptr<Texture>& texture) override;

		virtual void ReloadPipeline(std::shared_ptr<Pipeline>& pipeline) override;

		virtual void EnableImGui() override;
		virtual void ImGuiNewFrame() override;

		virtual void BindPipeline(const std::shared_ptr<Pipeline>& pipeline) override;
		virtual void BindVertexBuffer(std::shared_ptr<Buffer>& vertexBuffer) override;
		virtual void BindIndexBuffer(std::shared_ptr<Buffer>& indexBuffer) override;
		virtual void BindParameter(const std::string& parameterName, std::shared_ptr<Buffer>& buffer) override;
		virtual void BindParameter(const std::string& parameterName, std::shared_ptr<Texture>& texture, TextureUsage usage = kReadOnly) override;
		virtual void BindParameter(const std::string& parameterName, void* data, size_t size) override;

		virtual void BeginRender() override;
		virtual void BeginRenderPass(std::shared_ptr<RenderPass>& renderPass) override;
		virtual void SetSwapchainTarget(std::shared_ptr<RenderPass>& renderPass) override;
		virtual void EndRenderPass(std::shared_ptr<RenderPass>& renderPass) override;
		virtual void EndRender() override;

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t startVertexLocation = 0, uint32_t startInstanceLocation = 0) override;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t startIndexLocation = 0, uint32_t baseVertexLocation = 0, uint32_t startInstanceLocation = 0) override;
		virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

		virtual void Render() override;
		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual void ReadPixelFromTexture(uint32_t x, uint32_t y, std::shared_ptr<Texture> texture, glm::vec4& pixel) override;

	private:
		void PrepareDraw();
		void GetHardwareAdapter();
		void WaitForGPU();
		void CreateAttachments(std::shared_ptr<RenderPass>& renderPass);
		uint32_t GetRootParameterIndex(const std::string& parameterName);
		void CreateRootSignature(std::shared_ptr<Pipeline>& pipeline);
		ShaderResult CompileShader(std::filesystem::path filePath, ShaderStage stage);
		D3D12_STATIC_SAMPLER_DESC CreateSamplerDesc(uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility, D3D12_TEXTURE_ADDRESS_MODE addressMode);

		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(std::shared_ptr<DescriptorHeap> descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, int32_t offset = 0);
		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(std::shared_ptr<DescriptorHeap> descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, int32_t offset = 0);
		uint32_t AllocateHandle(std::shared_ptr<DescriptorHeap> descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		void CreateGraphicsPipeline(std::shared_ptr<Pipeline>& pipeline, std::shared_ptr<D3D12Pipeline>& internal_state, PipelineDesc* desc);
		void CreateComputePipeline(std::shared_ptr<Pipeline>& pipeline, std::shared_ptr<D3D12Pipeline>& internal_state, PipelineDesc* desc);

		void CreateCommands();

		void BindRootParameter(const std::string& parameterName, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

		void SetTextureData(std::shared_ptr<Texture>& texture);

		std::shared_ptr<DescriptorHeap> CreateDescriptorHeap(uint32_t num_descriptors, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
		void SetDescriptorHeap(std::shared_ptr<DescriptorHeap> descriptorHeap);
	};

	namespace Helpers
	{
		constexpr DXGI_FORMAT ConvertFormat(Format value)
		{
			switch (value)
			{
			case Format::UNKNOWN:
				return DXGI_FORMAT_UNKNOWN;
			case Format::R8_UNORM:
				return DXGI_FORMAT_R8_UNORM;
			case Format::R8_UINT:
				return DXGI_FORMAT_R8_UINT;
			case Format::R16_UINT:
				return DXGI_FORMAT_R16_UINT;
			case Format::R32_UINT:
				return DXGI_FORMAT_R32_UINT;
			case Format::R32_FLOAT:
				return DXGI_FORMAT_R32_FLOAT;
			case Format::RG8_UNORM:
				return DXGI_FORMAT_R8G8_UNORM;
			case Format::RG16_FLOAT:
				return DXGI_FORMAT_R16G16_FLOAT;
			case Format::RG32_FLOAT:
				return DXGI_FORMAT_R32G32_FLOAT;
			case Format::RGBA8_UNORM:
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			case Format::RGBA16_FLOAT:
				return DXGI_FORMAT_R16G16B16A16_FLOAT;
			case Format::RGBA32_FLOAT:
				return DXGI_FORMAT_R32G32B32A32_FLOAT;
			case Format::SRGB:
				return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

			// Depth Formats
			case Format::DEPTH32_FLOAT_STENCIL8X24_UINT:
				return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
			case Format::DEPTH32_FLOAT:
				return DXGI_FORMAT_D32_FLOAT;
			case Format::DEPTH24_STENCIL8:
				return DXGI_FORMAT_D24_UNORM_S8_UINT;

			// Typeless Formats
			case Format::R32_FLOAT_X8X24_TYPELESS:
				return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
			case Format::R32_TYPELESS:
				return DXGI_FORMAT_R32_TYPELESS;
			case Format::R24_X8_TYPELESS:
				return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			default:
				return DXGI_FORMAT_UNKNOWN;
			}
		}

		constexpr D3D12_BLEND ConvertBlend(Blend blend)
		{
			switch (blend)
			{
			case Blend::ZERO:
				return D3D12_BLEND_ZERO;
			case Blend::ONE:
				return D3D12_BLEND_ONE;
			case Blend::SRC_COLOR:
				return D3D12_BLEND_SRC_COLOR;
			case Blend::INV_SRC_COLOR:
				return D3D12_BLEND_INV_SRC_COLOR;
			case Blend::SRC_ALPHA:
				return D3D12_BLEND_SRC_ALPHA;
			case Blend::INV_SRC_ALPHA:
				return D3D12_BLEND_INV_SRC_ALPHA;
			case Blend::DEST_ALPHA:
				return D3D12_BLEND_DEST_ALPHA;
			case Blend::INV_DEST_ALPHA:
				return D3D12_BLEND_INV_DEST_ALPHA;
			case Blend::DEST_COLOR:
				return D3D12_BLEND_DEST_COLOR;
			case Blend::INV_DEST_COLOR:
				return D3D12_BLEND_INV_DEST_COLOR;
			case Blend::SRC_ALPHA_SAT:
				return D3D12_BLEND_SRC_ALPHA_SAT;
			case Blend::BLEND_FACTOR:
				return D3D12_BLEND_BLEND_FACTOR;
			case Blend::INV_BLEND_FACTOR:
				return D3D12_BLEND_INV_BLEND_FACTOR;
			case Blend::SRC1_COLOR:
				return D3D12_BLEND_SRC1_COLOR;
			case Blend::INV_SRC1_COLOR:
				return D3D12_BLEND_INV_SRC1_COLOR;
			case Blend::SRC1_ALPHA:
				return D3D12_BLEND_SRC1_ALPHA;
			case Blend::INV_SRC1_ALPHA:
				return D3D12_BLEND_INV_SRC1_ALPHA;
			default:
				return D3D12_BLEND_ZERO;
			}
		}

		constexpr D3D12_BLEND_OP ConvertBlendOp(BlendOp blendOp)
		{
			switch (blendOp)
			{
			case BlendOp::ADD:
				return D3D12_BLEND_OP_ADD;
			case BlendOp::SUBTRACT:
				return D3D12_BLEND_OP_SUBTRACT;
			case BlendOp::REV_SUBTRACT:
				return D3D12_BLEND_OP_REV_SUBTRACT;
			case BlendOp::MIN:
				return D3D12_BLEND_OP_MIN;
			case BlendOp::MAX:
				return D3D12_BLEND_OP_MAX;
			default:
				return D3D12_BLEND_OP_ADD;
			}
		}

		constexpr D3D12_COMPARISON_FUNC ConvertComparisonFunc(ComparisonFunc compFunc)
		{
			switch (compFunc)
			{
			case ComparisonFunc::NEVER:
				return D3D12_COMPARISON_FUNC_NEVER;
			case ComparisonFunc::LESS:
				return D3D12_COMPARISON_FUNC_LESS;
			case ComparisonFunc::EQUAL:
				return D3D12_COMPARISON_FUNC_EQUAL;
			case ComparisonFunc::LESS_EQUAL:
				return D3D12_COMPARISON_FUNC_LESS_EQUAL;
			case ComparisonFunc::GREATER:
				return D3D12_COMPARISON_FUNC_GREATER;
			case ComparisonFunc::NOT_EQUAL:
				return D3D12_COMPARISON_FUNC_NOT_EQUAL;
			case ComparisonFunc::GREATER_EQUAL:
				return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
			case ComparisonFunc::ALWAYS:
				return D3D12_COMPARISON_FUNC_ALWAYS;
			default:
				return D3D12_COMPARISON_FUNC_NEVER;
			}
		}

		constexpr D3D12_CULL_MODE ConvertCullMode(CullMode cull_mode)
		{
			switch (cull_mode)
			{
			case CullMode::NONE:
				return D3D12_CULL_MODE_NONE;
			case Eden::CullMode::FRONT:
				return D3D12_CULL_MODE_FRONT;
			case Eden::CullMode::BACK:
				return D3D12_CULL_MODE_BACK;
			default:
				return D3D12_CULL_MODE_NONE;
			}
		}

		constexpr D3D12_RESOURCE_STATES ConvertResourceState(ResourceState state)
		{
			switch (state)
			{
			case ResourceState::COMMON:
				return D3D12_RESOURCE_STATE_COMMON;
			case ResourceState::RENDER_TARGET:
				return D3D12_RESOURCE_STATE_RENDER_TARGET;
			case ResourceState::PRESENT:
				return D3D12_RESOURCE_STATE_PRESENT;
			case ResourceState::UNORDERED_ACCESS:
				return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			case ResourceState::PIXEL_SHADER:
				return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			case ResourceState::NON_PIXEL_SHADER:
				return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			case ResourceState::READ:
				return D3D12_RESOURCE_STATE_GENERIC_READ;
			case ResourceState::COPY_DEST:
				return D3D12_RESOURCE_STATE_COPY_DEST;
			case ResourceState::COPY_SRC:
				return D3D12_RESOURCE_STATE_COPY_SOURCE;
			case ResourceState::DEPTH_READ:
				return D3D12_RESOURCE_STATE_DEPTH_READ;
			case ResourceState::DEPTH_WRITE:
				return D3D12_RESOURCE_STATE_DEPTH_WRITE;
			default:
				return D3D12_RESOURCE_STATE_COMMON;
			}
		}
	}
}

