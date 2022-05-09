#include "GraphicsDevice.h"

#include <cstdint>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "Core/Memory.h"
#include "Utilities/Utils.h"
#include "Profiling/Profiler.h"

namespace Eden
{
	constexpr D3D_FEATURE_LEVEL s_featureLevel = D3D_FEATURE_LEVEL_12_1;

	GraphicsDevice::GraphicsDevice(HWND handle, uint32_t width, uint32_t height)
		: m_viewport(0.0f, 0.0f, (float)width, (float)height)
		, m_scissor(0, 0, width, height)
	{
		ED_PROFILE_FUNCTION("D3D12Device Init");

		uint32_t dxgiFactoryFlags = 0;

	#ifdef ED_DEBUG
		// NOTE: Enabling the debug layer after device creation will invalidate the active device.
		{
			ComPtr<ID3D12Debug> debugController;
			if (!FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();

				dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
		}
	#endif

		if (FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory))))
			ED_ASSERT_MB(false, "Failed to create dxgi factory!");

		GetHardwareAdapter();

		if (FAILED(D3D12CreateDevice(m_adapter.Get(), s_featureLevel, IID_PPV_ARGS(&m_device))))
			ED_ASSERT_MB(false, "Faile to create device");

		DXGI_ADAPTER_DESC1 adapterDesc;
		m_adapter->GetDesc1(&adapterDesc);
		std::string name;
		Utils::StringConvert(adapterDesc.Description, name);
		ED_LOG_INFO("Initialized D3D12 device on {}", name);

		// Create allocator
		D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
		allocatorDesc.pDevice = m_device.Get();
		allocatorDesc.pAdapter = m_adapter.Get();

		if (FAILED(D3D12MA::CreateAllocator(&allocatorDesc, &m_allocator)))
			ED_ASSERT_MB(false, "Failed to create allocator!");

		// Describe and create command queue
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		if (FAILED(m_device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_commandQueue))))
			ED_ASSERT_MB(false, "Failed to Create command queue!");

		// Describe and create the swap chain
		DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
		swapchainDesc.Width = width;
		swapchainDesc.Height = height;
		swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchainDesc.SampleDesc.Count = 1;
		swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainDesc.BufferCount = s_frameCount;
		swapchainDesc.Scaling = DXGI_SCALING_NONE;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

		ComPtr<IDXGISwapChain1> swapchain;
		if (FAILED(m_factory->CreateSwapChainForHwnd(m_commandQueue.Get(), handle, &swapchainDesc, nullptr, nullptr, &swapchain)))
			ED_ASSERT_MB(false, "Failed to create swapchain!");

		// NOTE(Daniel): Disable fullscreen
		m_factory->MakeWindowAssociation(handle, DXGI_MWA_NO_ALT_ENTER);

		swapchain.As(&m_swapchain);
		m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

		// Create descriptor heaps
		{
			// Describe and create the render target view(RTV) descriptor heap
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors = s_frameCount;
			rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

			if (FAILED(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap))))
				ED_ASSERT_MB(false, "Failed to create Render Target View descriptor heap!");

			m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			// Describe and create a shader resource view (SRV) heap for the texture.
			D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
			srvHeapDesc.NumDescriptors = 1;
			srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			if (FAILED(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap))))
				ED_ASSERT_MB(false, "Failed to create Shader Resource View descriptor heap!");
		}

		// Create frame resources
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

			// Create a RTV for each frame
			for (uint32_t i = 0; i < s_frameCount; ++i)
			{
				if (FAILED(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]))))
					ED_ASSERT_MB(false, "Failed to get render target from swapchain!");

				m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
				rtvHandle.Offset(1, m_rtvDescriptorSize);
			}
		}

		// Create command allocator
		if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator))))
		   ED_ASSERT_MB(false, "Failed to create command allocator!");

		// Create command list
		if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList))))
			ED_ASSERT_MB(false, "Failed to create command list!");

		// Command lists are created in the recording state, but there is nothing
		// to record yet. The main loop expects it to be closed, so close it now.
		m_commandList->Close();

		// Create synchronization objects
		{
			if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))))
				ED_ASSERT_MB(false, "Failed to create fence!");
			m_fenceValue = 1;

			// Create an event handler to use for frame synchronization
			m_fenceEvent = CreateEvent(nullptr, false, false, nullptr);
			ED_ASSERT_MB(m_fenceEvent != nullptr, HRESULT_FROM_WIN32(GetLastError()));
		}
	}

	GraphicsDevice::~GraphicsDevice()
	{
		WaitForPreviousFrame();

		m_vertexBufferAllocation->Release();
		m_cbAllocation->Release();
		m_textureAllocation->Release();

		CloseHandle(m_fenceEvent);
	}

	ComPtr<IDxcBlob> GraphicsDevice::CompileShader(std::filesystem::path filePath, ShaderTarget target)
	{
		std::wstring entryPoint = L"";
		std::wstring targetStr = L"";

		switch (target)
		{
		case Eden::ShaderTarget::Vertex:
			entryPoint = L"VSMain";
			targetStr = L"vs_6_0";
			break;
		case Eden::ShaderTarget::Pixel:
			entryPoint = L"PSMain";
			targetStr = L"ps_6_0";
			break;
		}

		std::vector<LPCWSTR> arguments =
		{
			filePath.c_str(),
			L"-E", entryPoint.c_str(),
			L"-T", targetStr.c_str(),
			L"-Zs"
		};

		ComPtr<IDxcBlobEncoding> source = nullptr;
		m_utils->LoadFile(filePath.c_str(), nullptr, &source);
		DxcBuffer sourceBuffer;
		sourceBuffer.Ptr = source->GetBufferPointer();
		sourceBuffer.Size = source->GetBufferSize();
		sourceBuffer.Encoding = DXC_CP_ACP;

		ComPtr<IDxcResult> result;
		m_compiler->Compile(&sourceBuffer, arguments.data(), (uint32_t)arguments.size(), m_includeHandler.Get(), IID_PPV_ARGS(&result));

		// Print errors if present.
		ComPtr<IDxcBlobUtf8> errors = nullptr;
		result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
		// Note that d3dcompiler would return null if no errors or warnings are present.  
		// IDxcCompiler3::Compile will always return an error buffer, but its length will be zero if there are no warnings or errors.
		if (errors != nullptr && errors->GetStringLength() != 0)
			ED_LOG_FATAL("Failed to compile {} {} shader: {}", filePath.filename(), Utils::ShaderTargetToString(target), errors->GetStringPointer());

		ComPtr<IDxcBlob> shaderBlob;
		result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);

		return shaderBlob;
	}

	void GraphicsDevice::CreateGraphicsPipeline(std::string shaderName)
	{
		// Create the root signature
		{
			D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

			if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
			{
				ED_LOG_WARN("Root signature version 1.1 not available, switching to 1.0");
				featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
			ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

			CD3DX12_ROOT_PARAMETER1 rootParameters[2];
			rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
			rootParameters[1].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_VERTEX);

			D3D12_STATIC_SAMPLER_DESC sampler = {};
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.MipLODBias = 0;
			sampler.MaxAnisotropy = 0;
			sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			sampler.MinLOD = 0.0f;
			sampler.MaxLOD = D3D12_FLOAT32_MAX;
			sampler.ShaderRegister = 0;
			sampler.RegisterSpace = 0;
			sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
			rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;

			if (FAILED(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error)))
				ED_ASSERT_MB(false, "Failed to serialize root signature");

			if (FAILED(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature))))
				ED_ASSERT_MB(false, "Failed to create root signature");
		}

		// Compile shaders and create pipeline state object(PSO)
		{
			
			DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_utils));
			DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_compiler));

			// Create default include handler
			m_utils->CreateDefaultIncludeHandler(&m_includeHandler);

			// Get shader file path
			std::wstring shaderPath = L"shaders/";
			shaderPath.append(std::wstring(shaderName.begin(), shaderName.end()));
			shaderPath.append(L".hlsl");

			auto vertexShader = CompileShader(shaderPath, ShaderTarget::Vertex);
			auto pixelShader = CompileShader(shaderPath, ShaderTarget::Pixel);

			// Define the vertex input layout
			D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// Describe and create pipeline state object(PSO)
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.pRootSignature = m_rootSignature.Get();
			psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
			psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
			psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			psoDesc.DepthStencilState.DepthEnable = false;
			psoDesc.DepthStencilState.StencilEnable = false;
			psoDesc.InputLayout.NumElements = ARRAYSIZE(inputElementDesc);
			psoDesc.InputLayout.pInputElementDescs = inputElementDesc;
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.SampleDesc.Count = 1;

			if (FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState))))
				ED_ASSERT_MB(false, "Failed to create graphics pipeline state!");
		}
	}

	void GraphicsDevice::CreateVertexBuffer(void* data, uint32_t size, uint32_t stride)
	{
		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Alignment = 0;
		resourceDesc.Width = 1024;
		resourceDesc.Height = 1024;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12MA::ALLOCATION_DESC allocationDesc = {};
		allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

		m_allocator->CreateResource(&allocationDesc,
									&CD3DX12_RESOURCE_DESC::Buffer(size),
									D3D12_RESOURCE_STATE_GENERIC_READ,
									nullptr,
									&m_vertexBufferAllocation,
									IID_PPV_ARGS(&m_vertexBuffer));

		// Copy the buffer data to the vertex buffer.
		UINT8* vertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		m_vertexBuffer->Map(0, &readRange, (void**)&vertexDataBegin);
		memcpy(vertexDataBegin, data, size);
		m_vertexBuffer->Unmap(0, nullptr);

		// Initialize the vertex buffer view.
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = stride;
		m_vertexBufferView.SizeInBytes = size;
	}

	void GraphicsDevice::CreateConstantBuffer(SceneData data)
	{
		D3D12MA::ALLOCATION_DESC allocationDesc = {};
		allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

		m_allocator->CreateResource(&allocationDesc,
									&CD3DX12_RESOURCE_DESC::Buffer(sizeof(SceneData)),
									D3D12_RESOURCE_STATE_GENERIC_READ,
									nullptr,
									&m_cbAllocation,
									IID_PPV_ARGS(&m_constantBuffer));

		// Map and initialize the constant buffer
		void* bufferData;
		m_constantBuffer->Map(0, nullptr, &bufferData);
		memcpy(bufferData, &data, sizeof(data));
		m_constantBuffer->Unmap(0, nullptr);
	}

	void GraphicsDevice::CreateTexture2D(std::string filePath)
	{
		m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get());

		ComPtr<ID3D12Resource> textureUploadHeap;
		D3D12MA::Allocation* uploadAllocation;

		// Create the texture
		{
			// Load texture from file
			int width, height, nchannels;
			unsigned char* texture = stbi_load(filePath.c_str(), &width, &height, &nchannels, 4);

			// Describe and create a texture2D
			D3D12_RESOURCE_DESC textureDesc = {};
			textureDesc.MipLevels = 1;
			textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			textureDesc.Width = width;
			textureDesc.Height = height;
			textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			textureDesc.DepthOrArraySize = 1;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.SampleDesc.Quality = 0;
			textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			D3D12MA::ALLOCATION_DESC allocationDesc = {};
			allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

			m_allocator->CreateResource(&allocationDesc,
										&textureDesc,
										D3D12_RESOURCE_STATE_COPY_DEST,
										nullptr,
										&m_textureAllocation,
										IID_PPV_ARGS(&m_texture));

			const uint64_t uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);

			// Create the GPU upload buffer
			D3D12MA::ALLOCATION_DESC uploadAllocationDesc = {};
			uploadAllocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

			auto hr = m_allocator->CreateResource(&uploadAllocationDesc,
												  &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
												  D3D12_RESOURCE_STATE_GENERIC_READ,
												  nullptr,
												  &uploadAllocation,
												  IID_PPV_ARGS(&textureUploadHeap));

			// Copy data to the intermediate upload heap and then schedule a copy 
			// from the upload heap to the Texture2D.
			D3D12_SUBRESOURCE_DATA textureData = {};
			textureData.pData = &texture[0];
			textureData.RowPitch = width * 4;
			textureData.SlicePitch = height * textureData.RowPitch;

			UpdateSubresources(m_commandList.Get(), m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
			m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

			// Describe and craete a Shader resource view(SRV) for the texture
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = textureDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());

			edelete texture;
		}

		m_commandList->Close();
		ID3D12CommandList* commandLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

		WaitForPreviousFrame();

		uploadAllocation->Release();
	}

	void GraphicsDevice::UpdateConstantBuffer(SceneData data)
	{
		// Map and initialize the constant buffer
		void* bufferData;
		m_constantBuffer->Map(0, nullptr, &bufferData);
		memcpy(bufferData, &data, sizeof(data));
		m_constantBuffer->Unmap(0, nullptr);
	}

	void GraphicsDevice::GetHardwareAdapter()
	{
		m_adapter = nullptr;

		ComPtr<IDXGIAdapter1> adapter;

		// Get IDXGIFactory6
		ComPtr<IDXGIFactory6> factory6;
		if (!FAILED(m_factory->QueryInterface(IID_PPV_ARGS(&factory6))))
		{
			for (uint32_t adapterIndex = 0;
				!FAILED(factory6->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)));
				 adapterIndex++)
			{
				DXGI_ADAPTER_DESC1 desc;
				adapter->GetDesc1(&desc);

				// Don't select the Basic Render Driver adapter.
				if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
					continue;

				// Check to see whether the adapter supports Direct3D 12, but don't create the
				// actual device yet.
				if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), s_featureLevel, _uuidof(ID3D12Device), nullptr)))
					break;
			}
		}
		else
		{
			ED_ASSERT_MB(false, "Failed to get IDXGIFactory6!");
		}

		ED_ASSERT_MB(adapter.Get() != nullptr, "Failed to get a compatible adapter!");

		 adapter.Swap(m_adapter);
	}

	void GraphicsDevice::Render()
	{
		ED_PROFILE_FUNCTION();

		// Record all the commands we need to render the scene into the command list
		PopulateCommandList();

		// Execute the command list
		ID3D12CommandList* commandLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(ARRAYSIZE(commandLists), commandLists);

		// Present the frame
		if (FAILED(m_swapchain->Present(1, 0)))
			ED_ASSERT_MB(false, "Failed to swapchain present!");

		WaitForPreviousFrame();
	}

	void GraphicsDevice::PopulateCommandList()
	{
		ED_PROFILE_FUNCTION();

		// Command list allocators can only be reseted when the associated
		// command lists have finished the execution on the GPU
		if (FAILED(m_commandAllocator->Reset()))
			ED_ASSERT_MB(false, "Failed to reset command allocator");

		// However, when ExecuteCommandList() is called on a particular command 
		// list, that command list can then be reset at any time and must be before 
		// re-recording.
		if (FAILED(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get())))
			ED_ASSERT_MB(false, "Failed to reset command list");

		m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

		ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
		m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

		m_commandList->SetGraphicsRootDescriptorTable(0, m_srvHeap->GetGPUDescriptorHandleForHeapStart());
		m_commandList->SetGraphicsRootConstantBufferView(1, m_constantBuffer->GetGPUVirtualAddress());

		m_commandList->RSSetViewports(1, &m_viewport);
		m_commandList->RSSetScissorRects(1, &m_scissor);

		// Indicate that the back buffer will be used as a render target
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
																				D3D12_RESOURCE_STATE_PRESENT,
																				D3D12_RESOURCE_STATE_RENDER_TARGET));

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
		m_commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

		// Record commands
		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
		m_commandList->DrawInstanced(3, 1, 0, 0);

		// Indicate that the back buffer will be used as present
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
																				D3D12_RESOURCE_STATE_RENDER_TARGET,
																				D3D12_RESOURCE_STATE_PRESENT));

		if (FAILED(m_commandList->Close()))
			ED_ASSERT_MB(false, "Failed to close command list");
	}

	void GraphicsDevice::WaitForPreviousFrame()
	{
		// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
		// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
		// sample illustrates how to use fences for efficient resource usage and to
		// maximize GPU utilization.

		// Signal and increment the fence value.
		const UINT64 fence = m_fenceValue;
		m_commandQueue->Signal(m_fence.Get(), fence);
		m_fenceValue++;

		// Wait until the previous frame is finished.
		if (m_fence->GetCompletedValue() < fence)
		{
			m_fence->SetEventOnCompletion(fence, m_fenceEvent);
			WaitForSingleObject(m_fenceEvent, INFINITE);
		}

		m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
	}

}