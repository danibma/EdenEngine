#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <filesystem>

#include "d3dx12.h"
#include <dxc/dxcapi.h>
#include <D3D12MemoryAllocator/D3D12MemAlloc.h>

#include "Core/Base.h"
#include "Core/Window.h"
#include <d3d12shader.h>

using namespace Microsoft::WRL;

namespace Eden
{
	constexpr uint32_t s_FrameCount = 2;

	enum ShaderStage
	{
		kVertex = 0,
		kPixel
	};

	namespace Utils
	{
		inline const char* ShaderStageToString(const ShaderStage target)
		{
			switch (target)
			{
			case kVertex:
				return "Vertex";
			case kPixel:
				return "Pixel";
			default:
				return "Null";
			}
		}
	}

	struct Resource
	{
		ComPtr<ID3D12Resource> resource;
		D3D12MA::Allocation* allocation;

		void Release()
		{
			if (resource != nullptr)
			{
				resource->Release();
				resource = nullptr;
				allocation->Release();
			}
		}
	};

	struct Buffer : Resource
	{
		void* data;
		uint32_t size;
		uint32_t stride;
		uint32_t count;
		uint32_t heap_offset; // Used for srv's, like structured buffer
	};

	struct Texture2D : Resource
	{
		uint64_t width;
		uint32_t height;
		DXGI_FORMAT format;
		uint32_t heap_offset;
	};

	struct PipelineState
	{
		bool enable_blending = false;
		D3D12_CULL_MODE cull_mode = D3D12_CULL_MODE_BACK;
		bool front_counter_clockwise = true;
		D3D12_COMPARISON_FUNC depth_func = D3D12_COMPARISON_FUNC_LESS;
		float min_depth = 0.0f;
	};

	struct Pipeline
	{
		enum PipelineType
		{
			kGraphics,
			kCompute // Not implemented
		};

		std::string name;
		PipelineType type = kGraphics;
		PipelineState draw_state;
		ComPtr<ID3D12RootSignature> root_signature;
		ComPtr<ID3D12PipelineState> pipeline_state;
		ComPtr<ID3D12ShaderReflection> pixel_reflection;
		ComPtr<ID3D12ShaderReflection> vertex_reflection;

		// name, rootParameterIndex
		std::unordered_map<std::string, size_t> root_parameter_indices;
	};

	class D3D12RHI
	{
		ComPtr<ID3D12Device> m_Device;
		ComPtr<IDXGIFactory4> m_Factory;
		ComPtr<IDXGIAdapter1> m_Adapter;
		ComPtr<ID3D12CommandQueue> m_CommandQueue;
		ComPtr<IDXGISwapChain3> m_Swapchain;
		ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
		uint32_t m_RTVDescriptorSize;
		ComPtr<ID3D12DescriptorHeap> m_DSVHeap;
		ComPtr<ID3D12DescriptorHeap> m_SRVHeap;
		ComPtr<ID3D12Resource> m_RenderTargets[s_FrameCount];
		ComPtr<ID3D12Resource> m_DepthStencil;
		ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
		ComPtr<ID3D12GraphicsCommandList> m_CommandList;
		ComPtr<ID3D12Fence> m_Fence;
		
		CD3DX12_RECT m_Scissor;
		CD3DX12_VIEWPORT m_Viewport;

		ComPtr<D3D12MA::Allocator> m_Allocator;

		bool m_ImguiInitialized = false;

		ComPtr<IDxcUtils> m_DxcUtils;
		ComPtr<IDxcCompiler3> m_DxcCompiler;
		ComPtr<IDxcIncludeHandler> m_DxcIncludeHandler;

		HANDLE m_FenceEvent;
		int32_t m_FrameIndex;
		uint64_t m_FenceValues[s_FrameCount];

		D3D12_VERTEX_BUFFER_VIEW m_BoundVertexBuffer;
		D3D12_INDEX_BUFFER_VIEW m_BoundIndexBuffer;
		Pipeline m_BoundPipeline;

		uint32_t m_SRVHeapOffset = 1;

		struct ShaderResult
		{
			ComPtr<IDxcBlob> blob;
			ComPtr<ID3D12ShaderReflection> reflection;
		};

	public:
		D3D12RHI(Window* window);
		~D3D12RHI();

		enum class BufferCreateSRV
		{
			kCreateSRV,
			kDontCreateSRV
		};

		// Template Helpers
		template<typename TYPE>
		Buffer CreateBuffer(void* data, uint32_t element_count, BufferCreateSRV create_srv = BufferCreateSRV::kDontCreateSRV)
		{
			Buffer buffer = CreateBuffer(element_count * sizeof(TYPE), data);
			buffer.stride = (uint32_t)sizeof(TYPE);
			buffer.count = element_count;

			if (create_srv == BufferCreateSRV::kCreateSRV)
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
				srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srv_desc.Format = DXGI_FORMAT_UNKNOWN;
				srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srv_desc.Buffer.FirstElement = 0;
				srv_desc.Buffer.NumElements = element_count;
				srv_desc.Buffer.StructureByteStride = sizeof(TYPE);
				srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

				buffer.heap_offset = m_SRVHeapOffset;
				CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_SRVHeap->GetCPUDescriptorHandleForHeapStart());
				m_Device->CreateShaderResourceView(buffer.resource.Get(), &srv_desc, handle.Offset(m_SRVHeapOffset, m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)));
				m_SRVHeapOffset++;
			}

			return buffer;
		}

		template<typename TYPE>
		void UpdateBuffer(Buffer& buffer, void* data, const uint32_t element_count)
		{
			memcpy(buffer.data, data, sizeof(TYPE) * element_count);
		}

		[[nodiscard]] Pipeline CreateGraphicsPipeline(std::string program_name, PipelineState draw_state = PipelineState());
		[[nodiscard]] Texture2D CreateTexture2D(std::string file_path);
		[[nodiscard]] Texture2D CreateTexture2D(unsigned char* texture_data, uint64_t width, uint32_t height);

		void EnableImGui();
		void ImGuiNewFrame();

		void BindPipeline(const Pipeline& pipeline);
		void BindVertexBuffer(Buffer vertex_buffer);
		void BindIndexBuffer(Buffer index_buffer);
		void BindConstantBuffer(std::string_view parameter_name, Buffer constant_buffer);
		void BindShaderResource(Buffer buffer);
		void BindShaderResource(Texture2D texture);
		void BindShaderResource(uint32_t heap_offset);

		void Draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t start_vertex_location = 0, uint32_t start_instance_location = 0);
		void DrawIndexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t start_index_location = 0, uint32_t base_vertex_location = 0, uint32_t start_instance_location = 0);

		void Render();
		void Resize(uint32_t width, uint32_t height);

		void ClearRenderTargets();

	private:
		void PrepareDraw();
		void GetHardwareAdapter();
		void WaitForGPU();
		void CreateBackBuffers(uint32_t width, uint32_t height);
		void CreateRootSignature(Pipeline& pipeline);
		Buffer CreateBuffer(uint32_t size, const void* data);
		ShaderResult CompileShader(std::filesystem::path file_path, ShaderStage stage);
		D3D12_STATIC_SAMPLER_DESC CreateStaticSamplerDesc(uint32_t shader_register, uint32_t register_space, D3D12_SHADER_VISIBILITY shader_visibility);
	};
}

