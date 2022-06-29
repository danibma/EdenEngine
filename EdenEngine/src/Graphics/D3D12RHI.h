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

// From Guillaume Boissé "gfx" https://github.com/gboisse/gfx/blob/b83878e562c2c205000b19c99cf24b13973dedb2/gfx_core.h#L77
#define ALIGN(VAL, ALIGN)   \
    (((VAL) + (static_cast<decltype(VAL)>(ALIGN) - 1)) & ~(static_cast<decltype(VAL)>(ALIGN) - 1))

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
		bool initialized_data = false;

		operator bool()
		{
			return resource;
		}
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

	struct DescriptorHeap
	{
		ComPtr<ID3D12DescriptorHeap> heap;
		bool is_set = false;
	};

	struct RenderPass
	{
		enum Type
		{
			kRenderTexture,
			kRenderTarget
		};

		std::shared_ptr<DescriptorHeap> rtv_heap;
		std::shared_ptr<DescriptorHeap> dsv_heap;
		D3D12_RENDER_PASS_RENDER_TARGET_DESC rtv_desc;
		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC dsv_desc;
		Texture2D render_targets[s_FrameCount];
		ComPtr<ID3D12Resource> depth_stencil;
		bool imgui = false;
		Type type;

		std::wstring name;
	};

	class D3D12RHI
	{
		ComPtr<ID3D12Device> m_Device;
		ComPtr<IDXGIFactory4> m_Factory;
		ComPtr<IDXGIAdapter1> m_Adapter;
		ComPtr<ID3D12CommandQueue> m_CommandQueue;
		ComPtr<IDXGISwapChain3> m_Swapchain;
		std::shared_ptr<DescriptorHeap> m_SRVHeap;
		ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
		ComPtr<ID3D12GraphicsCommandList4> m_CommandList;
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

		enum class BufferType
		{
			kDontCreateView,
			kCreateSRV,
			kCreateCBV
		};

		// Template Helpers
		template<typename TYPE>
		Buffer CreateBuffer(void* data, uint32_t element_count, BufferType view_creation = BufferType::kCreateCBV)
		{
			uint32_t size = element_count * sizeof(TYPE);
			if (view_creation == BufferType::kCreateCBV)
				size = ALIGN(size, 256);
			 
			Buffer buffer = CreateBuffer(size, data);
			buffer.stride = (uint32_t)sizeof(TYPE);
			buffer.count = element_count;

			switch (view_creation)
			{
			case BufferType::kCreateSRV:
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
					auto handle = GetCPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_SRVHeapOffset);
					m_Device->CreateShaderResourceView(buffer.resource.Get(), &srv_desc, handle);
					m_SRVHeapOffset++;
				}
				break;
			case BufferType::kCreateCBV:
				{
					D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
					cbv_desc.BufferLocation = buffer.resource->GetGPUVirtualAddress();
					cbv_desc.SizeInBytes = size;

					buffer.heap_offset = m_SRVHeapOffset;
					auto handle = GetCPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_SRVHeapOffset);
					m_Device->CreateConstantBufferView(&cbv_desc, handle);
					m_SRVHeapOffset++;
				}
				break;
			default:
				break;
			}

			return buffer;
		}

		template<typename TYPE>
		void UpdateBuffer(Buffer& buffer, void* data, const uint32_t element_count)
		{
			if (!buffer.initialized_data)
				buffer.resource->Map(0, nullptr, &buffer.data);
			memcpy(buffer.data, data, sizeof(TYPE) * element_count);
		}

		[[nodiscard]] Pipeline CreateGraphicsPipeline(std::string program_name, PipelineState draw_state = PipelineState());
		[[nodiscard]] Texture2D CreateTexture2D(std::string file_path);
		[[nodiscard]] Texture2D CreateTexture2D(unsigned char* texture_data, uint64_t width, uint32_t height);
		[[nodiscard]] RenderPass CreateRenderPass(RenderPass::Type type, std::wstring name = L"", bool imgui = false);

		void ReloadPipeline(Pipeline& pipeline);

		void EnableImGui();
		void ImGuiNewFrame();

		void BindPipeline(const Pipeline& pipeline);
		void BindVertexBuffer(Buffer vertex_buffer);
		void BindIndexBuffer(Buffer index_buffer);
		void BindParameter(const std::string& parameter_name,Buffer buffer);
		void BindParameter(const std::string& parameter_name,Texture2D texture);
		void BindParameter(const std::string& parameter_name,uint32_t heap_offset);

		void BeginRender();
		void BeginRenderPass(RenderPass& render_pass);
		void EndRenderPass(RenderPass& render_pass);
		void EndRender();

		void Draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t start_vertex_location = 0, uint32_t start_instance_location = 0);
		void DrawIndexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t start_index_location = 0, uint32_t base_vertex_location = 0, uint32_t start_instance_location = 0);

		void Render();
		void Resize(uint32_t width, uint32_t height);

		uint32_t GetCurrentFrameIndex();

		ID3D12Resource* GetCurrentRenderTarget(RenderPass& render_pass);
		void ChangeResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES current_state, D3D12_RESOURCE_STATES desired_state);

		D3D12_GPU_DESCRIPTOR_HANDLE GetTextureGPUHandle(Texture2D& texture);

		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(std::shared_ptr<DescriptorHeap> descriptor_heap, Texture2D texture);
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(std::shared_ptr<DescriptorHeap> descriptor_heap, Buffer buffer);
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(std::shared_ptr<DescriptorHeap> descriptor_heap, int32_t offset = 0);

		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(std::shared_ptr<DescriptorHeap> descriptor_heap, D3D12_DESCRIPTOR_HEAP_TYPE heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, int32_t offset = 0);

		std::shared_ptr<DescriptorHeap> CreateDescriptorHeap(uint32_t num_descriptors, D3D12_DESCRIPTOR_HEAP_TYPE descriptor_type, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
		void SetDescriptorHeap(std::shared_ptr<DescriptorHeap> descriptor_heap);

		void CreateBackBuffers(RenderPass& render_pass, uint32_t width, uint32_t height);
	private:
		void PrepareDraw();
		void GetHardwareAdapter();
		void WaitForGPU();
		size_t GetRootParameterIndex(const std::string& parameter_name);
		void CreateRootSignature(Pipeline& pipeline);
		Buffer CreateBuffer(uint32_t size, const void* data);
		ShaderResult CompileShader(std::filesystem::path file_path, ShaderStage stage);
		D3D12_STATIC_SAMPLER_DESC CreateStaticSamplerDesc(uint32_t shader_register, uint32_t register_space, D3D12_SHADER_VISIBILITY shader_visibility);
	};
}

