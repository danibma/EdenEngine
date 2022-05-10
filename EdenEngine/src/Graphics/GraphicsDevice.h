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

using namespace Microsoft::WRL;

namespace Eden
{
	constexpr uint32_t s_frameCount = 2;

	enum class ShaderTarget
	{
		Vertex,
		Pixel
	};

	// NOTE(Daniel): TEMP!!!!
	struct SceneData
	{
		glm::mat4 MVPMatrix;
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

	class GraphicsDevice
	{
		Window* m_window;

		ComPtr<ID3D12Device> m_device;
		ComPtr<IDXGIFactory4> m_factory;
		ComPtr<IDXGIAdapter1> m_adapter;
		ComPtr<ID3D12CommandQueue> m_commandQueue;
		ComPtr<IDXGISwapChain3> m_swapchain;
		ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
		ComPtr<ID3D12DescriptorHeap> m_srvHeap;
		ComPtr<ID3D12Resource> m_renderTargets[s_frameCount];
		ComPtr<ID3D12CommandAllocator> m_commandAllocator;
		ComPtr<ID3D12GraphicsCommandList> m_commandList;
		ComPtr<ID3D12Fence> m_fence;
		ComPtr<ID3D12PipelineState> m_pipelineState;
		ComPtr<ID3D12RootSignature> m_rootSignature;
		CD3DX12_VIEWPORT m_viewport;
		CD3DX12_RECT m_scissor;

		ComPtr<D3D12MA::Allocator> m_allocator;

		ComPtr<ID3D12Resource> m_vertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
		D3D12MA::Allocation* m_vertexBufferAllocation;

		ComPtr<ID3D12Resource> m_texture;
		D3D12MA::Allocation* m_textureAllocation;

		ComPtr<ID3D12Resource> m_constantBuffer;
		D3D12MA::Allocation* m_cbAllocation;
		void* m_constantBufferData;
		

		ComPtr<IDxcUtils> m_utils;
		ComPtr<IDxcCompiler3> m_compiler;
		ComPtr<IDxcIncludeHandler> m_includeHandler;

		HANDLE m_fenceEvent;

		uint64_t m_frameIndex;
		uint64_t m_fenceValues[s_frameCount];
		uint32_t m_rtvDescriptorSize;

	public:
		GraphicsDevice(Window* window);
		~GraphicsDevice();

		void CreateGraphicsPipeline(std::string shaderName);
		void CreateVertexBuffer(void* data, uint32_t size, uint32_t stride);
		void CreateConstantBuffer(SceneData data);
		void CreateTexture2D(std::string filePath);

		void UpdateConstantBuffer(SceneData data);

		void Render();

	private:
		void GetHardwareAdapter();
		void PopulateCommandList();
		// Prepare to render the next frame
		void MoveToNextFrame();
		// Wait for pending GPU work to complete
		void WaitForGPU();
		ComPtr<IDxcBlob> CompileShader(std::filesystem::path filePath, ShaderTarget target);
	};
}

