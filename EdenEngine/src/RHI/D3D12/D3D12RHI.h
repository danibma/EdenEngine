#pragma once

#include "Core/Base.h"
#include "Core/Assertions.h"
#include "Core/Window.h"
#include "RHI/RHI.h"

#include "D3D12Definitions.h"
#include "D3D12Device.h"

#include <filesystem>

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

	struct D3D12RenderPass : public ResourceInternal
	{
		std::vector<uint32_t> rtvDescriptorsIndices;
		uint32_t dsvDescriptorIndex;
	};

	struct D3D12GPUTimer : public ResourceInternal
	{
		ComPtr<ID3D12QueryHeap> queryHeap;
	};

	class D3D12RHI final : public IRHI
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
		void WaitForGPU();
		void CreateAttachments(RenderPass* renderPass);
		uint32_t GetRootParameterIndex(const std::string& parameterName);
		void CreateRootSignature(Pipeline* pipeline);
		ShaderResult CompileShader(std::filesystem::path filePath, ShaderStage stage);
		D3D12_STATIC_SAMPLER_DESC CreateSamplerDesc(uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility, D3D12_TEXTURE_ADDRESS_MODE addressMode);

		void CreateGraphicsPipeline(Pipeline* pipeline, PipelineDesc* desc);
		void CreateComputePipeline(Pipeline* pipeline, PipelineDesc* desc);

		void CreateCommands();

		void BindRootParameter(const std::string& parameterName, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

		void SetTextureData(Texture* texture);

		void BindSRVDescriptorHeap();
	};
}

