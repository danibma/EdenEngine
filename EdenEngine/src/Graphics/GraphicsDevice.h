#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl\client.h>
#include <filesystem>

#include "d3dx12.h"
#include <dxc/dxcapi.h>
#include <D3D12MemoryAllocator/D3D12MemAlloc.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Core/Base.h"
#include "Core\Window.h"
#include <d3d12shader.h>

using namespace Microsoft::WRL;

namespace Eden
{
	constexpr uint32_t s_frameCount = 2;

	enum ShaderStage
	{
		Vertex = 0,
		Pixel
	};

	namespace Utils
	{
		inline const char* ShaderStageToString(ShaderStage target)
		{
			switch (target)
			{
			case ShaderStage::Vertex:
				return "Vertex";
			case ShaderStage::Pixel:
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

		inline void Release()
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
		uint32_t heapOffset; // Used for srv's, like structured buffer
	};

	struct Texture2D : Resource
	{
		uint64_t width;
		uint32_t height;
		DXGI_FORMAT format;
		uint32_t heapOffset;
	};

	struct PipelineState
	{
		bool enableBlending = false;
		D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_BACK;
		bool frontCounterClockwise = true;
		D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_LESS;
		float minDepth = 0.0f;
	};

	struct Pipeline
	{
		enum PipelineType
		{
			Graphics,
			Compute // Not implemented
		};

		std::string name;
		PipelineType type = Graphics;
		PipelineState drawState;
		ComPtr<ID3D12RootSignature> rootSignature;
		ComPtr<ID3D12PipelineState> pipelineState;
		ComPtr<ID3D12ShaderReflection> pixelReflection;
		ComPtr<ID3D12ShaderReflection> vertexReflection;

		// name, rootParameterIndex
		std::unordered_map<std::string, uint32_t> rootParameterIndices;
	};

	class GraphicsDevice
	{
		ComPtr<ID3D12Device> m_device;
		ComPtr<IDXGIFactory4> m_factory;
		ComPtr<IDXGIAdapter1> m_adapter;
		ComPtr<ID3D12CommandQueue> m_commandQueue;
		ComPtr<IDXGISwapChain3> m_swapchain;
		ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
		uint32_t m_rtvDescriptorSize;
		ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
		ComPtr<ID3D12DescriptorHeap> m_srvHeap;
		ComPtr<ID3D12DescriptorHeap> m_texturesHeap;
		ComPtr<ID3D12Resource> m_renderTargets[s_frameCount];
		ComPtr<ID3D12Resource> m_depthStencil;
		ComPtr<ID3D12CommandAllocator> m_commandAllocator;
		ComPtr<ID3D12GraphicsCommandList> m_commandList;
		ComPtr<ID3D12Fence> m_fence;
		
		CD3DX12_RECT m_scissor;

		ComPtr<D3D12MA::Allocator> m_allocator;

		bool m_imguiInitialized = false;

		ComPtr<IDxcUtils> m_utils;
		ComPtr<IDxcCompiler3> m_compiler;
		ComPtr<IDxcIncludeHandler> m_includeHandler;

		HANDLE m_fenceEvent;
		int32_t m_frameIndex;
		uint64_t m_fenceValues[s_frameCount];

		D3D12_VERTEX_BUFFER_VIEW boundVertexBuffer;
		D3D12_INDEX_BUFFER_VIEW boundIndexBuffer;
		Pipeline boundPipeline;

		uint32_t m_srvHeapOffset = 1;

		struct ShaderResult
		{
			ComPtr<IDxcBlob> blob;
			ComPtr<ID3D12ShaderReflection> reflection;
		};

	public:
		CD3DX12_VIEWPORT m_viewport;
		GraphicsDevice(Window* window);
		~GraphicsDevice();

		// Template Helpers
		template<typename TYPE>
		Buffer CreateBuffer(void* data, uint32_t elementCount, bool createSrv = false)
		{
			Buffer buffer = CreateBuffer(elementCount * sizeof(TYPE), data);
			buffer.stride = (uint32_t)sizeof(TYPE);
			buffer.count = elementCount;

			if (createSrv)
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.Format = DXGI_FORMAT_UNKNOWN;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srvDesc.Buffer.FirstElement = 0;
				srvDesc.Buffer.NumElements = elementCount;
				srvDesc.Buffer.StructureByteStride = sizeof(TYPE);
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

				buffer.heapOffset = m_srvHeapOffset;
				CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_srvHeap->GetCPUDescriptorHandleForHeapStart());
				m_device->CreateShaderResourceView(buffer.resource.Get(), &srvDesc, handle.Offset(m_srvHeapOffset, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)));
				m_srvHeapOffset++;
			}

			return buffer;
		}

		template<typename TYPE>
		inline void UpdateBuffer(Buffer& buffer, void* data, uint32_t elementCount)
		{
			memcpy(buffer.data, data, sizeof(TYPE) * elementCount);
		}

		[[nodiscard]] Pipeline CreateGraphicsPipeline(std::string programName, PipelineState drawState = PipelineState());
		[[nodiscard]] Texture2D CreateTexture2D(std::string filePath);
		[[nodiscard]] Texture2D CreateTexture2D(unsigned char* textureData, uint64_t width, uint32_t height);

		void EnableImGui();
		void ImGuiNewFrame();

		void BindPipeline(const Pipeline& pipeline);
		void BindVertexBuffer(Buffer vertexBuffer);
		void BindIndexBuffer(Buffer indexBuffer);
		void BindConstantBuffer(std::string_view parameterName, Buffer constantBuffer);
		void BindShaderResource(Buffer buffer);
		void BindShaderResource(Texture2D texture);
		void BindShaderResource(uint32_t heapOffset);

		void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t startVertexLocation = 0, uint32_t startInstanceLocation = 0);
		void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t startIndexLocation = 0, uint32_t baseVertexLocation = 0, uint32_t startInstanceLocation = 0);

		void Render();
		void Resize(uint32_t width, uint32_t height);

		void ClearRenderTargets();

	private:
		void PrepareDraw();
		void GetHardwareAdapter();
		void WaitForGPU();
		void CreateBackBuffers(uint32_t width, uint32_t height);
		void CreateRootSignature(Pipeline& pipeline);
		Buffer CreateBuffer(uint32_t size, void* data);
		ShaderResult CompileShader(std::filesystem::path filePath, ShaderStage stage);
		D3D12_STATIC_SAMPLER_DESC CreateStaticSamplerDesc(uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility);
	};
}

