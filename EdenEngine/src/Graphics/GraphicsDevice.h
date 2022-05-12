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
		glm::mat4 modelMatrix;
		// This matrix is used to fix the problem of a uniform scale only changing the normal's magnitude and not it's direction
		glm::mat4 normalMatrix;
		glm::vec3 lightPosition;
		glm::vec3 viewPosition; // this is temporary, until we calculate lighting through view space instead of world space
		float padding[2];
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

	struct VertexBuffer
	{
		ComPtr<ID3D12Resource> buffer;
		D3D12_VERTEX_BUFFER_VIEW bufferView;
		D3D12MA::Allocation* allocation;
		uint32_t vertexCount;
	};

	struct IndexBuffer
	{
		ComPtr<ID3D12Resource> buffer;
		D3D12_INDEX_BUFFER_VIEW bufferView;
		D3D12MA::Allocation* allocation;
		uint32_t indexCount;
	};

	struct Texture2D
	{
		ComPtr<ID3D12Resource> texture;
		D3D12MA::Allocation* allocation;
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
		ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
		ComPtr<ID3D12DescriptorHeap> m_srvHeap;
		ComPtr<ID3D12Resource> m_renderTargets[s_frameCount];
		ComPtr<ID3D12Resource> m_depthStencil;
		ComPtr<ID3D12CommandAllocator> m_commandAllocator;
		ComPtr<ID3D12GraphicsCommandList> m_commandList;
		ComPtr<ID3D12Fence> m_fence;
		ComPtr<ID3D12PipelineState> m_pipelineState;
		ComPtr<ID3D12RootSignature> m_rootSignature;
		CD3DX12_VIEWPORT m_viewport;
		CD3DX12_RECT m_scissor;

		ComPtr<D3D12MA::Allocator> m_allocator;

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
		VertexBuffer CreateVertexBuffer(void* data, uint32_t vertexCount, uint32_t stride);
		IndexBuffer CreateIndexBuffer(void* data, uint32_t indexCount, uint32_t stride);
		Texture2D CreateTexture2D(std::string filePath);
		void CreateConstantBuffer(SceneData data);

		void UpdateConstantBuffer(SceneData data);

		void Render();

		VertexBuffer vertexBuffer;
		IndexBuffer indexBuffer;
		uint32_t textureHeapOffset = 1;

	private:
		void GetHardwareAdapter();

		void PopulateCommandList();

		// Prepare to render the next frame
		void MoveToNextFrame();

		// Wait for pending GPU work to complete
		void WaitForGPU();

		ComPtr<IDxcBlob> CompileShader(std::filesystem::path filePath, ShaderTarget target);

		D3D12_STATIC_SAMPLER_DESC CreateStaticSamplerDesc(uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility);
		void CreateRootSignature();
	};
}

