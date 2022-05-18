#include "GraphicsDevice.h"

#include <cstdint>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <imgui/backends/imgui_impl_dx12.h>
#include <imgui/backends/imgui_impl_win32.h>

#include "Core/Memory.h"
#include "Utilities/Utils.h"
#include "Profiling/Profiler.h"

namespace Eden
{
	constexpr D3D_FEATURE_LEVEL s_featureLevel = D3D_FEATURE_LEVEL_12_1;

	GraphicsDevice::GraphicsDevice(Window* window)
		: m_viewport(0.0f, 0.0f, (float)window->GetWidth(), (float)window->GetHeight())
		, m_scissor(0, 0, window->GetWidth(), window->GetHeight())
		, m_fenceValues{}
	{
		uint32_t dxgiFactoryFlags = 0;

	#ifdef ED_DEBUG
		// NOTE: Enabling the debug layer after device creation will invalidate the active device.
		{
			ComPtr<ID3D12Debug> debugController;
			if (!FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

				ComPtr<ID3D12Debug1> debugController1;
				if (FAILED(debugController->QueryInterface(IID_PPV_ARGS(&debugController1))))
					ED_LOG_WARN("Failed to get ID3D12Debug1!");

				debugController1->SetEnableGPUBasedValidation(true);
			}
		}
	#endif

		if (FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory))))
			ED_ASSERT_MB(false, "Failed to create dxgi factory!");

		GetHardwareAdapter();

		if (FAILED(D3D12CreateDevice(m_adapter.Get(), s_featureLevel, IID_PPV_ARGS(&m_device))))
			ED_ASSERT_MB(false, "Faile to create device");

		m_device->SetName(L"gfx_device");

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
		swapchainDesc.Width = window->GetWidth();
		swapchainDesc.Height = window->GetHeight();
		swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchainDesc.SampleDesc.Count = 1;
		swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainDesc.BufferCount = s_frameCount;
		swapchainDesc.Scaling = DXGI_SCALING_NONE;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

		ComPtr<IDXGISwapChain1> swapchain;
		if (FAILED(m_factory->CreateSwapChainForHwnd(m_commandQueue.Get(), window->GetHandle(), &swapchainDesc, nullptr, nullptr, &swapchain)))
			ED_ASSERT_MB(false, "Failed to create swapchain!");

		// NOTE(Daniel): Disable fullscreen
		m_factory->MakeWindowAssociation(window->GetHandle(), DXGI_MWA_NO_ALT_ENTER);

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

			// Describe and create the depth stencil view(DSV) descriptor heap
			D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
			dsvHeapDesc.NumDescriptors = 1;
			dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

			if (FAILED(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap))))
				ED_ASSERT_MB(false, "Failed to create Depth Stencil View descriptor heap!");

			// Describe and create a shader resource view (SRV) heap for the texture.
			D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
			srvHeapDesc.NumDescriptors = 128;
			srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			if (FAILED(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap))))
				ED_ASSERT_MB(false, "Failed to create Shader Resource View descriptor heap!");
		}

		CreateBackBuffers(window->GetWidth(), window->GetHeight());

		// Create command allocator
		if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator))))
		   ED_ASSERT_MB(false, "Failed to create command allocator!");

		m_commandAllocator->SetName(L"gfx_command_allocator");

		// Create command list
		if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList))))
			ED_ASSERT_MB(false, "Failed to create command list!");

		m_commandList->SetName(L"gfx_command_list");

		// Create the depth stencil view.
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
			depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
			depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

			D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
			depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
			depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
			depthOptimizedClearValue.DepthStencil.Stencil = 0;

			if (FAILED(m_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, window->GetWidth(), window->GetHeight(), 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&depthOptimizedClearValue,
				IID_PPV_ARGS(&m_depthStencil)
			)))
			{
				ED_ASSERT_MB(false, "Failed to create depth stencil buffer");
			}

			m_device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
		}

		// Create synchronization objects
		{
			if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))))
				ED_ASSERT_MB(false, "Failed to create fence!");
			m_fenceValues[m_frameIndex]++;

			// Create an event handler to use for frame synchronization
			m_fenceEvent = CreateEvent(nullptr, false, false, nullptr);
			ED_ASSERT_MB(m_fenceEvent != nullptr, HRESULT_FROM_WIN32(GetLastError()));

			// Wait for the command list to execute; we are reusing the same command 
			// list in our main loop but for now, we just want to wait for setup to 
			// complete before continuing.
			WaitForGPU();
		}

		// Associate the graphics device Resize with the window resize callback
		window->SetResizeCallback([&](uint32_t x, uint32_t y) { Resize(x, y); });

		// Setup profiler
		ID3D12Device* pDevice = m_device.Get();
		ID3D12CommandQueue* pCommandQueue = m_commandQueue.Get();
		ED_PROFILE_GPU_INIT(pDevice, &pCommandQueue, 1);

		// Indicate that the back buffer will be used as a render target
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
																				D3D12_RESOURCE_STATE_PRESENT,
																				D3D12_RESOURCE_STATE_RENDER_TARGET));
	}

	GraphicsDevice::~GraphicsDevice()
	{
		WaitForGPU();

		if (m_imguiInitialized)
		{
			ImGui_ImplDX12_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
		}

		CloseHandle(m_fenceEvent);
	}

	ComPtr<IDxcBlob> GraphicsDevice::CompileShader(std::filesystem::path filePath, ShaderTarget target)
	{
		ED_PROFILE_FUNCTION();

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
			ED_LOG_ERROR("Failed to compile {} {} shader: {}", filePath.filename(), Utils::ShaderTargetToString(target), errors->GetStringPointer());

		ComPtr<IDxcBlob> shaderBlob;
		result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);

	#if 0
		// Get reflection blob
		ComPtr<IDxcBlob> reflectionBlob;
		result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionBlob), nullptr);

		DxcBuffer reflectionBuffer;
		reflectionBuffer.Ptr = reflectionBlob->GetBufferPointer();
		reflectionBuffer.Size = reflectionBlob->GetBufferSize();
		reflectionBuffer.Encoding = DXC_CP_ACP;

		m_utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&m_pixelReflection));

		D3D12_SHADER_DESC shaderDesc;
		m_pixelReflection->GetDesc(&shaderDesc);
		for (uint32_t i = 0; i < shaderDesc.BoundResources; ++i)
		{
			D3D12_SHADER_INPUT_BIND_DESC desc = {};
			m_pixelReflection->GetResourceBindingDesc(i, &desc);

			ED_LOG_INFO("Shader Resource {}:{}", i, desc.Name);
		}

		for (uint32_t i = 0; i < shaderDesc.InputParameters; ++i)
		{
			D3D12_SIGNATURE_PARAMETER_DESC desc = {};
			m_pixelReflection->GetInputParameterDesc(i, &desc);

			ED_LOG_INFO("Shader Input Parameter {}:{}", i, desc.SemanticName);
		}
	#endif

		return shaderBlob;
	}

	D3D12_STATIC_SAMPLER_DESC GraphicsDevice::CreateStaticSamplerDesc(uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility)
	{
		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = shaderRegister;
		sampler.RegisterSpace = registerSpace;
		sampler.ShaderVisibility = shaderVisibility;

		return sampler;
	}

	void GraphicsDevice::CreateRootSignature(Pipeline& pipeline)
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			ED_LOG_WARN("Root signature version 1.1 not available, switching to 1.0");
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		CD3DX12_DESCRIPTOR_RANGE1 ranges[1] = {};
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

		CD3DX12_ROOT_PARAMETER1 rootParameters[2] = {};
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);

		D3D12_STATIC_SAMPLER_DESC samplerDiffuse = CreateStaticSamplerDesc(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
		D3D12_STATIC_SAMPLER_DESC samplers[] = { samplerDiffuse };

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, _countof(samplers), samplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;

		if (FAILED(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error)))
			ED_LOG_ERROR("Failed to serialize root signature");

		if (FAILED(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&pipeline.rootSignature))))
			ED_LOG_ERROR("Failed to create root signature");
	}

	Buffer GraphicsDevice::CreateBuffer(uint64_t size, void* data)
	{
		Buffer buffer = {};
		buffer.size = size;

		D3D12MA::ALLOCATION_DESC allocationDesc = {};
		allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

		m_allocator->CreateResource(&allocationDesc,
									&CD3DX12_RESOURCE_DESC::Buffer(size),
									D3D12_RESOURCE_STATE_GENERIC_READ,
									nullptr,
									&buffer.allocation,
									IID_PPV_ARGS(&buffer.resource));

		buffer.resource->Map(0, nullptr, &buffer.data);
		memcpy(buffer.data, data, size);
		buffer.resource->Unmap(0, nullptr);

		return buffer;
	}

	void GraphicsDevice::CreateBackBuffers(uint32_t width, uint32_t height)
{
		// Create render targets
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame
		for (uint32_t i = 0; i < s_frameCount; ++i)
		{
			if (FAILED(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]))))
				ED_ASSERT_MB(false, "Failed to get render target from swapchain!");

			m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);

			m_renderTargets[i]->SetName(L"backbuffer");
		}

		// Create DSV
		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
		depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

		D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
		depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
		depthOptimizedClearValue.DepthStencil.Stencil = 0;

		if (FAILED(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&m_depthStencil)
		)))
		{
			ED_ASSERT_MB(false, "Failed to create depth stencil buffer");
		}

		m_device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	}

	Pipeline GraphicsDevice::CreateGraphicsPipeline(std::string programName)
	{
		ED_PROFILE_FUNCTION();

		Pipeline pipeline = {};
		pipeline.type = Pipeline::Graphics;

		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_utils));
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_compiler));

		// Create default include handler
		m_utils->CreateDefaultIncludeHandler(&m_includeHandler);

		// Get shader file path
		std::wstring shaderPath = L"shaders/";
		shaderPath.append(std::wstring(programName.begin(), programName.end()));
		shaderPath.append(L".hlsl");

		auto vertexShader = CompileShader(shaderPath, ShaderTarget::Vertex);
		auto pixelShader = CompileShader(shaderPath, ShaderTarget::Pixel);

		ED_LOG_INFO("Compiled program {} successfully!", programName);

		// TODO: Implement shader reflection
		// Create the root signature
		CreateRootSignature(pipeline);

		// Define the vertex input layout
		D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,   0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// Describe and create pipeline state object(PSO)
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = pipeline.rootSignature.Get();
		psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
		psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		psoDesc.RasterizerState.FrontCounterClockwise = true;
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.InputLayout.NumElements = ARRAYSIZE(inputElementDesc);
		psoDesc.InputLayout.pInputElementDescs = inputElementDesc;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		if (FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline.pipelineState))))
			ED_ASSERT_MB(false, "Failed to create graphics pipeline state!");

		return pipeline;
	}

	Texture2D GraphicsDevice::CreateTexture2D(std::string filePath)
	{
		ED_PROFILE_FUNCTION();

		Texture2D texture;

		ComPtr<ID3D12Resource> textureUploadHeap;
		D3D12MA::Allocation* uploadAllocation;

		ED_PROFILE_GPU_CONTEXT(m_commandList.Get());
		ED_PROFILE_GPU_FUNCTION("GPU::CreateTexture2D");

		// Create the texture
		{
			// Load texture from file
			int width, height, nchannels;
			unsigned char* textureFile = stbi_load(filePath.c_str(), &width, &height, &nchannels, 4);
			if (!textureFile)
			{
				ED_LOG_ERROR("Could not load texture file: {}", filePath);
				return Texture2D();
			}
				
			texture.width = width;
			texture.height = height;
			texture.format = DXGI_FORMAT_R8G8B8A8_UNORM;

			// Describe and create a texture2D
			D3D12_RESOURCE_DESC textureDesc = {};
			textureDesc.MipLevels = 1;
			textureDesc.Format = texture.format;
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
										&texture.allocation,
										IID_PPV_ARGS(&texture.resource));

			const uint64_t uploadBufferSize = GetRequiredIntermediateSize(texture.resource.Get(), 0, 1);

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
			textureData.pData = &textureFile[0];
			textureData.RowPitch = width * 4;
			textureData.SlicePitch = height * textureData.RowPitch;

			UpdateSubresources(m_commandList.Get(), texture.resource.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
			m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

			edelete textureFile;
		}

		m_commandList->Close();
		ID3D12CommandList* commandLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
		m_commandList->Reset(m_commandAllocator.Get(), nullptr);

		// Describe and craete a Shader resource view(SRV) for the texture
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texture.format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		// NOTE(Daniel): Offset srv handle cause of imgui
		texture.heapOffset = textureHeapOffset;
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_srvHeap->GetCPUDescriptorHandleForHeapStart());
		m_device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, handle.Offset(textureHeapOffset, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)));

		WaitForGPU();

		uploadAllocation->Release();

		textureHeapOffset++;

		return texture;
	}

	void GraphicsDevice::EnableImGui()
	{
		// Setup imgui
		ImGui_ImplDX12_Init(m_device.Get(), s_frameCount, DXGI_FORMAT_R8G8B8A8_UNORM, m_srvHeap.Get(), m_srvHeap->GetCPUDescriptorHandleForHeapStart(), m_srvHeap->GetGPUDescriptorHandleForHeapStart());
		m_imguiInitialized = true;

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void GraphicsDevice::ImGuiNewFrame()
	{
		// Update and Render additional Platform Windows
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void GraphicsDevice::BindPipeline(const Pipeline& pipeline)
	{
		ED_PROFILE_FUNCTION();
		m_commandList->SetPipelineState(pipeline.pipelineState.Get());
		m_commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
		boundPipeline = pipeline;
	}

	void GraphicsDevice::BindVertexBuffer(Buffer vertexBuffer)
	{
		ED_PROFILE_FUNCTION();
		ED_ASSERT_LOG(vertexBuffer.resource != nullptr, "Can't bind a empty vertex buffer!");
		boundVertexBuffer.BufferLocation = vertexBuffer.resource->GetGPUVirtualAddress();
		boundVertexBuffer.SizeInBytes	 = vertexBuffer.size;
		boundVertexBuffer.StrideInBytes	 = vertexBuffer.stride;
	}

	void GraphicsDevice::BindIndexBuffer(Buffer indexBuffer)
	{
		ED_PROFILE_FUNCTION();
		ED_ASSERT_LOG(indexBuffer.resource != nullptr, "Can't bind a empty index buffer!");
		boundIndexBuffer.BufferLocation = indexBuffer.resource->GetGPUVirtualAddress();
		boundIndexBuffer.SizeInBytes	= indexBuffer.size;
		boundIndexBuffer.Format			= DXGI_FORMAT_R32_UINT;
	}

	void GraphicsDevice::BindConstantBuffer(uint32_t rootParameterIndex, Buffer constantBuffer)
	{
		ED_PROFILE_FUNCTION();
		m_commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, constantBuffer.resource->GetGPUVirtualAddress());
	}

	void GraphicsDevice::BindTexture2D(Texture2D texture)
	{
		ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
		m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

		// TODO(Daniel): Offset srv handle cause of imgui, make a descriptor heap for imgui only
		CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(m_srvHeap->GetGPUDescriptorHandleForHeapStart());
		srvHandle.Offset(texture.heapOffset, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		m_commandList->SetGraphicsRootDescriptorTable(0, srvHandle);
	}

	void GraphicsDevice::Draw(uint32_t vertexCount, uint32_t instanceCount /*= 1*/, uint32_t startVertexLocation /*= 0*/, uint32_t startInstanceLocation /*= 0*/)
	{
		ED_ASSERT_LOG(boundPipeline.pipelineState != nullptr, "Can't Draw without a valid pipeline bound!");
		ED_ASSERT_LOG(boundPipeline.rootSignature != nullptr, "Can't Draw without a valid pipeline bound!");

		ED_PROFILE_FUNCTION();

		ED_PROFILE_GPU_CONTEXT(m_commandList.Get());
		ED_PROFILE_GPU_FUNCTION("GPU::Draw");

		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
		m_commandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		m_commandList->RSSetViewports(1, &m_viewport);
		m_commandList->RSSetScissorRects(1, &m_scissor);

		m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		m_commandList->IASetVertexBuffers(0, 1, &boundVertexBuffer);
		m_commandList->DrawInstanced(vertexCount, instanceCount, startVertexLocation, startInstanceLocation);
	}

	void GraphicsDevice::DrawIndexed(uint32_t indexCount, uint32_t instanceCount /*= 1*/, uint32_t startIndexLocation /*= 0*/, uint32_t baseVertexLocation /*= 0*/, uint32_t startInstanceLocation /*= 0*/)
	{
		ED_ASSERT_LOG(boundPipeline.pipelineState != nullptr, "Can't DrawIndexed without a valid pipeline bound!");
		ED_ASSERT_LOG(boundPipeline.rootSignature != nullptr, "Can't DrawIndexed without a valid pipeline bound!");

		ED_PROFILE_FUNCTION();

		ED_PROFILE_GPU_CONTEXT(m_commandList.Get());
		ED_PROFILE_GPU_FUNCTION("GPU::DrawIndexed");

		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
		m_commandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

		m_commandList->RSSetViewports(1, &m_viewport);
		m_commandList->RSSetScissorRects(1, &m_scissor);

		m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_commandList->IASetVertexBuffers(0, 1, &boundVertexBuffer);
		m_commandList->IASetIndexBuffer(&boundIndexBuffer);

		m_commandList->DrawIndexedInstanced(indexCount, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
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

		if (m_imguiInitialized)
		{
			ImGui::Render();
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());
		}

		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
																				D3D12_RESOURCE_STATE_RENDER_TARGET,
																				D3D12_RESOURCE_STATE_PRESENT));

		if (FAILED(m_commandList->Close()))
			ED_ASSERT_MB(false, "Failed to close command list");

		ID3D12CommandList* commandLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(ARRAYSIZE(commandLists), commandLists);

		// Schedule a Signal command in the queue
		const uint64_t currentFenceValue = m_fenceValues[m_frameIndex];
		m_commandQueue->Signal(m_fence.Get(), currentFenceValue);

		ED_PROFILE_GPU_FLIP(m_swapchain.Get());
		m_swapchain->Present(1, 0);

		// Update the frame index
		m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

		// If the next frame is not ready to be rendered yet, wait until it is ready
		if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
		{
			m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
			WaitForSingleObject(m_fenceEvent, INFINITE); // wait for GPU to complete
		}

		// Set the fence value for the next frame
		m_fenceValues[m_frameIndex] = currentFenceValue + 1;

		WaitForGPU(); // NOTE(Daniel): this shouldn't be needed here, fix this!

		// Command list allocators can only be reseted when the associated
		// command lists have finished the execution on the GPU
		m_commandAllocator->Reset();

		// However, when ExecuteCommandList() is called on a particular command 
		// list, that command list can then be reset at any time and must be before 
		// re-recording.
		m_commandList->Reset(m_commandAllocator.Get(), nullptr);

		// Indicate that the back buffer will be used as a render target
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
																				D3D12_RESOURCE_STATE_PRESENT,
																				D3D12_RESOURCE_STATE_RENDER_TARGET));

		if (m_imguiInitialized)
			ImGuiNewFrame();
	}

	void GraphicsDevice::Resize(uint32_t width, uint32_t height)
	{
		if (m_device && m_swapchain)
		{
			ED_LOG_TRACE("Resizing Window to {}x{}", width, height);

			WaitForGPU();
			if (FAILED(m_commandList->Close()))
				ED_ASSERT_MB(false, "Failed to close command list");

			for (UINT n = 0; n < s_frameCount; n++)
			{
				m_renderTargets[n].Reset();
				m_fenceValues[n] = m_fenceValues[m_frameIndex];
			}

			m_depthStencil.Reset();

			m_swapchain->ResizeBuffers(s_frameCount, width, height, DXGI_FORMAT_UNKNOWN, 0);

			m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

			CreateBackBuffers(width, height);

			m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)width, (float)height);
			m_scissor  = CD3DX12_RECT(0, 0, width, height);

			m_commandAllocator->Reset();
			m_commandList->Reset(m_commandAllocator.Get(), nullptr);

			// Indicate that the back buffer will be used as a render target
			m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
																					D3D12_RESOURCE_STATE_PRESENT,
																					D3D12_RESOURCE_STATE_RENDER_TARGET));
		}
	}

	void GraphicsDevice::ClearRenderTargets()
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);

		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	}

	void GraphicsDevice::WaitForGPU()
	{
		// Schedule a signal command in the queue
		m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]);

		// Wait until the fence has been processed
		m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
		WaitForSingleObject(m_fenceEvent, INFINITE);

		// Increment the fence value for the current frame
		m_fenceValues[m_frameIndex]++;
	}
}
