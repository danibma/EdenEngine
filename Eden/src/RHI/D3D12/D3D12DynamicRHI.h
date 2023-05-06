#pragma once

#include "Core/Base.h"
#include "Core/Assertions.h"
#include "Core/Window.h"
#include "RHI/DynamicRHI.h"

#include "D3D12Definitions.h"
#include "D3D12Device.h"

#include <filesystem>

namespace Eden
{
	struct D3D12Resource
	{
		ComPtr<ID3D12Resource> resource;
		ComPtr<D3D12MA::Allocation> allocation;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;

		virtual ~D3D12Resource() {}
	};

	struct D3D12Buffer : public D3D12Resource, Buffer
	{
		//! Created in case we need things for the buffer
		
		~D3D12Buffer() {}
	};

	struct D3D12Texture : public D3D12Resource, Texture
	{
		ComPtr<ID3D12Resource> uploadHeap;
		ComPtr<D3D12MA::Allocation> uploadAllocation;
		std::vector<D3D12_SUBRESOURCE_DATA> data;

		~D3D12Texture() {}
	};

	struct D3D12Pipeline : public Pipeline
	{
		ComPtr<ID3D12RootSignature> rootSignature;
		ComPtr<ID3D12PipelineState> pipelineState;
		ComPtr<ID3D12ShaderReflection> pixel_reflection;
		ComPtr<ID3D12ShaderReflection> vertexReflection;
		ComPtr<ID3D12ShaderReflection> computeReflection;

		~D3D12Pipeline() {}
	};

	struct D3D12RenderPass : public RenderPass
	{
		std::vector<uint32_t> rtvDescriptorsIndices;
		uint32_t dsvDescriptorIndex;

		~D3D12RenderPass() {}
	};

	struct D3D12GPUTimer : public GPUTimer
	{
		ComPtr<ID3D12QueryHeap> queryHeap;

		~D3D12GPUTimer() {}
	};

	class D3D12DynamicRHI final : public DynamicRHI
	{
		D3D12Device* m_Device;
		ComPtr<IDXGISwapChain3> m_Swapchain;
		ComPtr<ID3D12Fence> m_Fence;
		ComPtr<ID3D12CommandQueue> m_CommandQueue;
		ComPtr<ID3D12CommandAllocator> m_CommandAllocator[GFrameCount];
		ComPtr<ID3D12GraphicsCommandList4> m_CommandList;
		CD3DX12_RECT m_Scissor;

		// Buffer used to get the pixel from the "ObjectPicker" color attachment
		D3D12Resource pixelReadStagingBuffer;

		bool m_bIsImguiInitialized = false;

		ComPtr<IDxcUtils> m_DxcUtils;
		ComPtr<IDxcCompiler3> m_DxcCompiler;
		ComPtr<IDxcIncludeHandler> m_DxcIncludeHandler;

		HANDLE m_FenceEvent;
		uint64_t m_FenceValues[GFrameCount] = { 0, 0 };

		D3D12_VERTEX_BUFFER_VIEW m_BoundVertexBuffer;
		D3D12_INDEX_BUFFER_VIEW m_BoundIndexBuffer;
		PipelineRef m_BoundPipeline;

		struct ShaderResult
		{
			ComPtr<IDxcBlob> blob;
			ComPtr<ID3D12ShaderReflection> reflection;
		};

		RenderPassRef m_SwapchainTarget;

		// temp until I found another solution
		PipelineRef m_MipsPipeline;

	public:
		virtual void Init(Window* window) override;
		virtual void Shutdown() override;

		virtual BufferRef     CreateBuffer(BufferDesc* desc, const void* initial_data) override;
		virtual PipelineRef   CreatePipeline(PipelineDesc* desc) override;
		virtual TextureRef    CreateTexture(std::string path, bool bGenerateMips) override;
		virtual TextureRef    CreateTexture(TextureDesc* desc) override;
		virtual RenderPassRef CreateRenderPass(RenderPassDesc* desc) override;
		virtual GPUTimerRef   CreateGPUTimer() override;

		virtual void BeginGPUTimer(GPUTimerRef timer) override;
		virtual void EndGPUTimer(GPUTimerRef timer) override;

		virtual void UpdateBufferData(BufferRef buffer, const void* data, uint32_t count = 0) override;
		virtual void GenerateMips(TextureRef texture) override;

		virtual void ChangeResourceState(TextureRef resource, ResourceState currentState, ResourceState desiredState, int subresource = -1) override;
		virtual void EnsureMsgResourceState(TextureRef resource, ResourceState destResourceState) override;

		virtual uint64_t GetTextureID(TextureRef texture) override;

		virtual void ReloadPipeline(PipelineRef pipeline) override;

		virtual void EnableImGui() override;
		virtual void ImGuiNewFrame() override;

		virtual void BindPipeline(PipelineRef pipeline) override;
		virtual void BindVertexBuffer(BufferRef vertexBuffer) override;
		virtual void BindIndexBuffer(BufferRef indexBuffer) override;
		virtual void BindParameter(const std::string& parameterName, BufferRef buffer) override;
		virtual void BindParameter(const std::string& parameterName, TextureRef texture, TextureUsage usage = kReadOnly) override;
		virtual void BindParameter(const std::string& parameterName, void* data, size_t size) override; // Use only for constants

		virtual void BeginRender() override;
		virtual void BeginRenderPass(RenderPassRef renderPass) override;
		virtual void SetSwapchainTarget(RenderPassRef renderPass) override;
		virtual void EndRenderPass(RenderPassRef renderPass) override;
		virtual void EndRender() override;

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t startVertexLocation = 0, uint32_t startInstanceLocation = 0) override;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t startIndexLocation = 0, uint32_t baseVertexLocation = 0, uint32_t startInstanceLocation = 0) override;
		virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

		virtual void Render() override;
		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual void ReadPixelFromTexture(uint32_t x, uint32_t y, TextureRef texture, glm::vec4& pixel) override;

	private:
		void PrepareDraw();
		void WaitForGPU();
		void CreateAttachments(RenderPassRef renderPass);
		uint32_t GetRootParameterIndex(const std::string& parameterName);
		void CreateRootSignature(PipelineRef pipeline);
		ShaderResult CompileShader(std::filesystem::path filePath, ShaderStage stage);
		D3D12_STATIC_SAMPLER_DESC CreateSamplerDesc(uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility, D3D12_TEXTURE_ADDRESS_MODE addressMode);

		void CreateGraphicsPipeline(PipelineRef pipeline, PipelineDesc* desc);
		void CreateComputePipeline(PipelineRef pipeline, PipelineDesc* desc);

		void CreateCommands();

		void BindRootParameter(const std::string& parameterName, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

		void SetTextureData(TextureRef texture);

		void BindSRVDescriptorHeap();
	};
}

