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

	enum class ShaderTarget
	{
		Vertex,
		Pixel
	};

	namespace Utils
	{
		inline const char* ShaderTargetToString(ShaderTarget target)
		{
			switch (target)
			{
			case ShaderTarget::Vertex:
				return "Vertex";
			case ShaderTarget::Pixel:
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
				allocation->Release();
			}
		}
	};

	struct VertexBuffer : Resource
	{
		D3D12_VERTEX_BUFFER_VIEW bufferView;
		uint32_t vertexCount;
	};

	struct IndexBuffer : Resource
	{
		D3D12_INDEX_BUFFER_VIEW bufferView;
		uint32_t indexCount;
	};

	struct Texture2D : Resource
	{
		uint32_t width;
		uint32_t height;
		DXGI_FORMAT format;
	};

	struct Pipeline
	{
		enum PipelineType
		{
			Graphics,
			Compute // Not implemented
		};

		PipelineType type;
		ComPtr<ID3D12RootSignature> rootSignature;
		ComPtr<ID3D12PipelineState> pipelineState;

		struct Parameter
		{
			std::string name;
			struct
			{
				void* data = nullptr;
				size_t dataSize = 0;
				Resource resource;
			} paramData;
			

			inline void UnsetData()
			{
				paramData.resource.Release();
			}

			inline void SetData(D3D12MA::Allocator* allocator, const void* data, size_t dataSize)
			{
				if (dataSize == paramData.dataSize)
				{
					memcpy(paramData.data, data, dataSize);
				}
				else
				{
					UnsetData();

					D3D12MA::ALLOCATION_DESC allocationDesc = {};
					allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

					allocator->CreateResource(&allocationDesc,
											  &CD3DX12_RESOURCE_DESC::Buffer(dataSize),
											  D3D12_RESOURCE_STATE_GENERIC_READ,
											  nullptr,
											  &paramData.resource.allocation,
											  IID_PPV_ARGS(&paramData.resource.resource));

					paramData.resource.resource->Map(0, nullptr, &paramData.data);
					memcpy(paramData.data, data, dataSize);
					paramData.dataSize = dataSize;
				}
			}
		};

		std::unordered_map<std::string, Parameter> parameters;

		inline Parameter& CreateParameter(const char* parameterName)
		{
			auto it = parameters.find(parameterName);
			if (it == parameters.end())
			{
				Parameter& parameter = parameters[parameterName];
				parameter.name = parameterName;

				return parameter;
			}
			return it->second;
		}

		inline void Release()
		{
			for (auto& p : parameters)
			{
				auto& [data, parameter] = p;
				parameter.UnsetData();
			}
		}
	};

	class GraphicsDevice
	{
		Window* m_window;

		ComPtr<ID3D12Device> m_device;
		ComPtr<IDXGIFactory4> m_factory;
		ComPtr<IDXGIAdapter1> m_adapter;
		ComPtr<ID3D12CommandQueue> m_commandQueue;
		ComPtr<IDXGISwapChain3> m_swapchain;
		ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
		uint32_t m_rtvDescriptorSize;
		ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
		ComPtr<ID3D12DescriptorHeap> m_srvHeap;
		ComPtr<ID3D12Resource> m_renderTargets[s_frameCount];
		ComPtr<ID3D12Resource> m_depthStencil;
		ComPtr<ID3D12CommandAllocator> m_commandAllocator;
		ComPtr<ID3D12GraphicsCommandList> m_commandList;
		ComPtr<ID3D12Fence> m_fence;
		CD3DX12_VIEWPORT m_viewport;
		CD3DX12_RECT m_scissor;

		ComPtr<D3D12MA::Allocator> m_allocator;

		bool m_imguiInitialized = false;

		ComPtr<IDxcUtils> m_utils;
		ComPtr<IDxcCompiler3> m_compiler;
		ComPtr<IDxcIncludeHandler> m_includeHandler;

		HANDLE m_fenceEvent;
		int32_t m_frameIndex;
		uint64_t m_fenceValues[s_frameCount];

		VertexBuffer boundVertexBuffer;
		IndexBuffer	 boundIndexBuffer;
		Pipeline	 boundPipeline;

		uint32_t textureHeapOffset = 1;

	public:
		GraphicsDevice(Window* window);
		~GraphicsDevice();

		template<typename Type> inline void SetPipelineParameter(Pipeline& pipeline, const char* parameterName, const Type& data)
		{
			pipeline.CreateParameter(parameterName).SetData(m_allocator.Get(), &data, sizeof(data));
		}

		[[nodiscard]] Pipeline CreateGraphicsPipeline(std::string programName);
		[[nodiscard]] VertexBuffer CreateVertexBuffer(void* data, uint32_t vertexCount, uint32_t stride);
		[[nodiscard]] IndexBuffer CreateIndexBuffer(void* data, uint32_t indexCount, uint32_t stride);
		[[nodiscard]] Texture2D CreateTexture2D(std::string filePath);
		void EnableImGui();
		void BindPipeline(const Pipeline& pipeline);
		void BindVertexBuffer(VertexBuffer vertexBuffer);
		void BindIndexBuffer(IndexBuffer indexBuffer);
		void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t startVertexLocation = 0, uint32_t startInstanceLocation = 0);
		void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t startIndexLocation = 0, uint32_t baseIndexLocation = 0, uint32_t startInstanceLocation = 0);
		void Render();

	private:
		void GetHardwareAdapter();
		void WaitForGPU();
		ComPtr<IDxcBlob> CompileShader(std::filesystem::path filePath, ShaderTarget target);
		D3D12_STATIC_SAMPLER_DESC CreateStaticSamplerDesc(uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility);
		void CreateRootSignature(Pipeline& pipeline);
	};
}

