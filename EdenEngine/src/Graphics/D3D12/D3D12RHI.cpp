#include "D3D12RHI.h"

#include <cstdint>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <imgui/backends/imgui_impl_dx12.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/ImGuizmo.h>

#include <dxgidebug.h>
#include <dxgi1_6.h>

#include "Utilities/Utils.h"
#include "Profiling/Profiler.h"
#include <WinPixEventRuntime/pix3.h>

// From Guillaume Boisse "gfx" https://github.com/gboisse/gfx/blob/b83878e562c2c205000b19c99cf24b13973dedb2/gfx_core.h#L77
#define ALIGN(VAL, ALIGN)   \
    (((VAL) + (static_cast<decltype(VAL)>(ALIGN) - 1)) & ~(static_cast<decltype(VAL)>(ALIGN) - 1))

namespace Eden
{
	// Globals
	constexpr D3D_FEATURE_LEVEL s_FeatureLevel = D3D_FEATURE_LEVEL_12_1;

	// Internal Helpers
	D3D12Resource* ToInternal(const Resource* resource)
	{
		return static_cast<D3D12Resource*>(resource->internal_state.get());
	}

	D3D12Buffer* ToInternal(const Buffer* buffer)
	{
		return static_cast<D3D12Buffer*>(buffer->internal_state.get());
	}

	D3D12Texture* ToInternal(const Texture* texture)
	{
		return static_cast<D3D12Texture*>(texture->internal_state.get());
	}

	D3D12Pipeline* ToInternal(const Pipeline* pipeline)
	{
		return static_cast<D3D12Pipeline*>(pipeline->internal_state.get());
	}

	D3D12RenderPass* ToInternal(const RenderPass* renderPass)
	{
		return static_cast<D3D12RenderPass*>(renderPass->internal_state.get());
	}

	D3D12GPUTimer* ToInternal(const GPUTimer* gpuTimer)
	{
		return static_cast<D3D12GPUTimer*>(gpuTimer->internal_state.get());
	}

	// Debug message callback
	void DebugMessageCallback(D3D12_MESSAGE_CATEGORY Category,
							  D3D12_MESSAGE_SEVERITY Severity,
							  D3D12_MESSAGE_ID ID,
							  LPCSTR pDescription,
							  void* pContext)
	{
		switch (Severity)
		{
		case D3D12_MESSAGE_SEVERITY_CORRUPTION:
			ED_LOG_FATAL("{}", pDescription);
			break;
		case D3D12_MESSAGE_SEVERITY_ERROR:
			ED_LOG_ERROR("{}", pDescription);
			break;
		case D3D12_MESSAGE_SEVERITY_WARNING:
			ED_LOG_WARN("{}", pDescription);
			break;
		case D3D12_MESSAGE_SEVERITY_INFO:
			ED_LOG_INFO("{}", pDescription);
			break;
		case D3D12_MESSAGE_SEVERITY_MESSAGE:
			ED_LOG_TRACE("{}", pDescription);
			break;
		}
	}

	GfxResult D3D12RHI::Init(Window* window)
	{
		m_Scissor = CD3DX12_RECT(0, 0, window->GetWidth(), window->GetHeight());
		m_CurrentAPI = kApi_D3D12;

		uint32_t dxgiFactoryFlags = 0;

	#ifdef ED_DEBUG
		// NOTE: Enabling the debug layer after device creation will invalidate the active device.
		{
			ComPtr<ID3D12Debug> debugController;
			if (!FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();

				ComPtr<ID3D12Debug1> debugController1;
				if (FAILED(debugController->QueryInterface(IID_PPV_ARGS(&debugController1))))
					ED_LOG_WARN("Failed to get ID3D12Debug1!");

				debugController1->SetEnableGPUBasedValidation(true);
			}

			ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
			if (!FAILED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue))))
			{
				dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

				dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
				dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
			}
		}
	#endif

		if (FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_Factory))))
		{
			ensureMsg(false, "Failed to create dxgi factory!");
			return GfxResult::kInternalError;
		}

		GetHardwareAdapter();

		if (FAILED(D3D12CreateDevice(m_Adapter.Get(), s_FeatureLevel, IID_PPV_ARGS(&m_Device))))
		{
			ensureMsg(false, "Faile to create device");
			return GfxResult::kInternalError;
		}

	#ifdef ED_DEBUG
		{
			if (!FAILED(m_Device->QueryInterface(IID_PPV_ARGS(&m_InfoQueue))))
			{
				ComPtr<ID3D12InfoQueue1> infoQueue1;
				
				if (!FAILED(m_InfoQueue->QueryInterface(IID_PPV_ARGS(&infoQueue1))))
					infoQueue1->RegisterMessageCallback(DebugMessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &m_MessageCallbackCookie);
			}
		}
	#endif

		m_Device->SetName(L"gfxDevice");

		DXGI_ADAPTER_DESC1 adapterDesc;
		m_Adapter->GetDesc1(&adapterDesc);
		std::string name;
		Utils::StringConvert(adapterDesc.Description, name);
		ED_LOG_INFO("Initialized D3D12 device on {}", name);

		// Create allocator
		D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
		allocatorDesc.pDevice = m_Device.Get();
		allocatorDesc.pAdapter = m_Adapter.Get();

		if (FAILED(D3D12MA::CreateAllocator(&allocatorDesc, &m_Allocator)))
		{
			ensureMsg(false, "Failed to create allocator!");
			return GfxResult::kInternalError;
		}

		// Create commands
		CreateCommands();

		// Describe and create the swap chain
		DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
		swapchainDesc.Width = window->GetWidth();
		swapchainDesc.Height = window->GetHeight();
		swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchainDesc.SampleDesc.Count = 1;
		swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainDesc.BufferCount = s_FrameCount;
		swapchainDesc.Scaling = DXGI_SCALING_NONE;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

		ComPtr<IDXGISwapChain1> swapchain;
		if (FAILED(m_Factory->CreateSwapChainForHwnd(m_CommandQueue.Get(), window->GetHandle(), &swapchainDesc, nullptr, nullptr, &swapchain))) 
		{
			ensureMsg(false, "Failed to create swapchain!");
			return GfxResult::kInternalError;
		}

		// NOTE(Daniel): Disable fullscreen
		m_Factory->MakeWindowAssociation(window->GetHandle(), DXGI_MWA_NO_ALT_ENTER);

		swapchain.As(&m_Swapchain);
		m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

		// Describe and create a shader resource view (SRV) heap for the texture.
		CreateDescriptorHeap(&m_SRVHeap, s_SRVDescriptorCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

		// Create synchronization objects
		{
			if (FAILED(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)))) 
			{
				ensureMsg(false, "Failed to create fence!");
				return GfxResult::kInternalError;
			}
			m_FenceValues[m_FrameIndex]++;

			// Create an event handler to use for frame synchronization
			m_FenceEvent = CreateEvent(nullptr, false, false, nullptr);
			ensureMsg(m_FenceEvent != nullptr, Utils::GetLastErrorMessage());

			// Wait for the command list to execute; we are reusing the same command 
			// list in our main loop but for now, we just want to wait for setup to 
			// complete before continuing.
			WaitForGPU();
		}

		// Associate the graphics device Resize with the window resize callback
		window->SetResizeCallback([&](uint32_t x, uint32_t y) 
		{ 
			GfxResult error = Resize(x, y); 
			ensure(error == GfxResult::kNoError);
		});

		// Setup profiler
		ID3D12Device* pDevice = m_Device.Get();
		ID3D12CommandQueue* pCommandQueue = m_CommandQueue.Get();
		ED_PROFILE_GPU_INIT(pDevice, &pCommandQueue, 1);

		// Create mips pipeline
		PipelineDesc mipsDesc = {};
		mipsDesc.programName = "GenerateMips";
		mipsDesc.type = kPipelineType_Compute;
		CreatePipeline(&m_MipsPipeline, &mipsDesc);

		return GfxResult::kNoError;
	}

	void D3D12RHI::Shutdown()
	{
		WaitForGPU();

		if (m_bIsImguiInitialized)
		{
			ImGui_ImplDX12_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
		}

		CloseHandle(m_FenceEvent);

		ED_PROFILER_SHUTDOWN();
	}

	D3D12RHI::ShaderResult D3D12RHI::CompileShader(std::filesystem::path filePath, ShaderStage stage)
	{
		ED_PROFILE_FUNCTION();

		std::wstring entryPoint = L"";
		std::wstring stageStr = L"";

		switch (stage)
		{
		case ShaderStage::kVS:
			entryPoint = L"VSMain";
			stageStr = L"vs_6_0";
			break;
		case ShaderStage::kPS:
			entryPoint = L"PSMain";
			stageStr = L"ps_6_0";
			break;
		case ShaderStage::kCS:
			entryPoint = L"CSMain";
			stageStr = L"cs_6_0";
			break;
		}

		std::wstring wpdbName = filePath.stem().wstring() + L"_" + stageStr + L".pdb";

		std::vector arguments =
		{
			filePath.c_str(),
			L"-E", entryPoint.c_str(),
			L"-T", stageStr.c_str(),
			L"-Zi",
			L"-Fd", wpdbName.c_str()
		};

		ComPtr<IDxcBlobEncoding> source = nullptr;
		m_DxcUtils->LoadFile(filePath.c_str(), nullptr, &source);
		if (!source)
		{
			ED_LOG_ERROR("Could not find the program shader file: {}", filePath);
			ensure(false);
		}

		DxcBuffer sourceBuffer;
		sourceBuffer.Ptr = source->GetBufferPointer();
		sourceBuffer.Size = source->GetBufferSize();
		sourceBuffer.Encoding = DXC_CP_ACP;

		ComPtr<IDxcResult> result;
		m_DxcCompiler->Compile(&sourceBuffer, arguments.data(), (uint32_t)arguments.size(), m_DxcIncludeHandler.Get(), IID_PPV_ARGS(&result));

		// Print errors if present.
		ComPtr<IDxcBlobUtf8> errors = nullptr;
		result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
		// Note that d3dcompiler would return null if no errors or warnings are present.  
		// IDxcCompiler3::Compile will always return an error buffer, but its length will be zero if there are no warnings or errors.
		if (errors != nullptr && errors->GetStringLength() != 0)
			ED_LOG_ERROR("Failed to compile {} {} shader: {}", filePath.filename(), Utils::ShaderStageToString(stage), errors->GetStringPointer());

		// Save pdb
#if defined(ED_PROFILING)
		ComPtr<IDxcBlob> pdb = nullptr;
		ComPtr<IDxcBlobUtf16> pdbName = nullptr;
		result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pdb), &pdbName);
		{
			if (!std::filesystem::exists("shaders/intermediate/"))
				std::filesystem::create_directory("shaders/intermediate");

			std::wstring pdbPath = L"shaders/intermediate/";
			pdbPath.append(pdbName->GetStringPointer());

			FILE* fp = NULL;
			_wfopen_s(&fp, pdbPath.c_str(), L"wb");
			fwrite(pdb->GetBufferPointer(), pdb->GetBufferSize(), 1, fp);
			fclose(fp);
		}
#endif

		// Get shader blob
		ComPtr<IDxcBlob> shaderBlob;
		result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);

		// Get reflection blob
		ComPtr<IDxcBlob> reflectionBlob;
		result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionBlob), nullptr);

		DxcBuffer reflectionBuffer;
		reflectionBuffer.Ptr = reflectionBlob->GetBufferPointer();
		reflectionBuffer.Size = reflectionBlob->GetBufferSize();
		reflectionBuffer.Encoding = DXC_CP_ACP;

		ComPtr<ID3D12ShaderReflection> reflection;
		m_DxcUtils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&reflection));

		return { shaderBlob, reflection };
	}

	D3D12_STATIC_SAMPLER_DESC D3D12RHI::CreateSamplerDesc(uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility, D3D12_TEXTURE_ADDRESS_MODE addressMode)
	{
		D3D12_STATIC_SAMPLER_DESC sampler;
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = addressMode;
		sampler.AddressV = sampler.AddressU;
		sampler.AddressW = sampler.AddressU;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MipLODBias = 0;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = shaderRegister;
		sampler.RegisterSpace = registerSpace;
		sampler.ShaderVisibility = shaderVisibility;

		return sampler;
	}

	void D3D12RHI::CreateRootSignature(Pipeline* pipeline)
	{
		auto internal_state = ToInternal(pipeline);

		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(m_Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			ED_LOG_WARN("Root signature version 1.1 not available, switching to 1.0");
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		uint32_t srvCount = 0;
		std::vector<D3D12_ROOT_PARAMETER1> rootParameters;
		std::vector<D3D12_DESCRIPTOR_RANGE1> descriptorRanges;
		std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;
		
		std::vector<uint32_t> shaderCounts;
		if (internal_state->vertexReflection)
			shaderCounts.emplace_back(uint32_t(ShaderStage::kVS));
		if (internal_state->pixel_reflection)
			shaderCounts.emplace_back(uint32_t(ShaderStage::kPS));
		if (internal_state->computeReflection)
			shaderCounts.emplace_back(uint32_t(ShaderStage::kCS));
		uint32_t shaderCount = static_cast<uint32_t>(shaderCounts.size());

		for (uint32_t i = shaderCounts[0]; i < shaderCounts[0] + shaderCount; ++i)
		{
			ComPtr<ID3D12ShaderReflection> reflection;
			D3D12_SHADER_VISIBILITY shaderVisibility;

			// Get shader stage
			switch (ShaderStage(i))
			{
			case ShaderStage::kVS:
				reflection = internal_state->vertexReflection;
				shaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
				break;
			case ShaderStage::kPS:
				reflection = internal_state->pixel_reflection;
				shaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
				break;
			case ShaderStage::kCS:
				reflection = internal_state->computeReflection;
				shaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				break;
			default:
				ED_LOG_ERROR("Shader stage is not yet implemented!");
				break;
			}

			if (!reflection) return;
			D3D12_SHADER_DESC shaderDesc = {};
			reflection->GetDesc(&shaderDesc);

			for (uint32_t j = 0; j < shaderDesc.BoundResources; ++j)
			{
				D3D12_SHADER_INPUT_BIND_DESC desc = {};
				reflection->GetResourceBindingDesc(j, &desc);

				// NOTE(Daniel): Add more as needed
				switch (desc.Type)
				{
				case D3D_SIT_STRUCTURED:
				case D3D_SIT_TEXTURE:
					{
						auto rootParameterIndex = pipeline->rootParameterIndices.find(desc.Name);
						bool bWasRootParameterFound = rootParameterIndex != pipeline->rootParameterIndices.end();
						if (bWasRootParameterFound) continue;

						D3D12_DESCRIPTOR_RANGE1 descriptorRange = {};
						descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
						descriptorRange.NumDescriptors = desc.BindCount;
						descriptorRange.BaseShaderRegister = desc.BindPoint;
						descriptorRange.RegisterSpace = desc.Space;
						descriptorRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE; // check if this is needed
						descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
						descriptorRanges.emplace_back(descriptorRange);

						D3D12_ROOT_PARAMETER1 rootParameter = {};
						rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
						rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

						pipeline->rootParameterIndices[desc.Name] = static_cast<uint32_t>(rootParameters.size()); // get the root parameter index
						rootParameters.emplace_back(rootParameter);
					}
					break;
				case D3D_SIT_CBUFFER:
					{
						D3D12_ROOT_PARAMETER1 rootParameter = {};

						ID3D12ShaderReflectionConstantBuffer* constantBuffer = reflection->GetConstantBufferByName(desc.Name);
						D3D12_SHADER_BUFFER_DESC bufferDesc = {};
						constantBuffer->GetDesc(&bufferDesc);

						if (bufferDesc.Variables == 1)
						{
							ID3D12ShaderReflectionVariable* rootVariable = constantBuffer->GetVariableByIndex(0);
							D3D12_SHADER_VARIABLE_DESC variableDesc = {};
							rootVariable->GetDesc(&variableDesc);

							D3D12_ROOT_CONSTANTS constants;
							constants.ShaderRegister = desc.BindPoint;
							constants.RegisterSpace = desc.Space;
							constants.Num32BitValues = variableDesc.Size / sizeof(uint32_t);

							rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
							rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
							rootParameter.Constants = constants;

							descriptorRanges.emplace_back(); // Emtpy descriptor range
						}
						else if (bufferDesc.Variables > 1)
						{
							auto rootParameterIndex = pipeline->rootParameterIndices.find(desc.Name);
							bool bWasRootParameterFound = rootParameterIndex != pipeline->rootParameterIndices.end();
							if (bWasRootParameterFound) continue;

							D3D12_DESCRIPTOR_RANGE1 descriptorRange = {};
							descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
							descriptorRange.NumDescriptors = desc.BindCount;
							descriptorRange.BaseShaderRegister = desc.BindPoint;
							descriptorRange.RegisterSpace = desc.Space;
							descriptorRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
							descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
							descriptorRanges.emplace_back(descriptorRange);

							rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
							rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
						}

						pipeline->rootParameterIndices[desc.Name] = static_cast<uint32_t>(rootParameters.size()); // get the root parameter index
						rootParameters.emplace_back(rootParameter);
					}
					break;
				case D3D_SIT_UAV_RWTYPED:
					{
						auto rootParameterIndex = pipeline->rootParameterIndices.find(desc.Name);
						bool bWasRootParameterFound = rootParameterIndex != pipeline->rootParameterIndices.end();
						if (bWasRootParameterFound) continue;

						D3D12_DESCRIPTOR_RANGE1 descriptorRange = {};
						descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
						descriptorRange.NumDescriptors = desc.BindCount;
						descriptorRange.BaseShaderRegister = desc.BindPoint;
						descriptorRange.RegisterSpace = desc.Space;
						descriptorRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE; // check if this is needed
						descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
						descriptorRanges.emplace_back(descriptorRange);

						D3D12_ROOT_PARAMETER1 rootParameter = {};
						rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
						rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
						pipeline->rootParameterIndices[desc.Name] = static_cast<uint32_t>(rootParameters.size()); // get the root parameter index
						rootParameters.emplace_back(rootParameter);
					}
					break;
				case D3D_SIT_SAMPLER:
					{
						// Right now is only creating linear samplers
						//! This gets the samplers on Global.hlsli, don't create samplers on a specific shader
						D3D12_STATIC_SAMPLER_DESC sampler;
						std::string samplerName = std::string(desc.Name);
						if (samplerName == "LinearClamp")
							sampler = CreateSamplerDesc(desc.BindPoint, desc.Space, shaderVisibility, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
						else if (samplerName == "LinearWrap")
							sampler = CreateSamplerDesc(desc.BindPoint, desc.Space, shaderVisibility, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
						samplers.emplace_back(sampler);
					}
					break;
				default:
					ED_LOG_ERROR("Resource type has not been yet implemented!");
					break;
				}
			}
		}

		for (size_t i = 0; i < rootParameters.size(); ++i)
		{
			auto& parameter = rootParameters[i];
			if (parameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) continue;
			parameter.DescriptorTable.pDescriptorRanges = &descriptorRanges[i];
			parameter.DescriptorTable.NumDescriptorRanges = 1;
		}

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
		uint32_t num_rootParameters = static_cast<uint32_t>(rootParameters.size());
		uint32_t num_samplers = static_cast<uint32_t>(samplers.size());
		rootSignatureDesc.Init_1_1(num_rootParameters, rootParameters.data(), num_samplers, samplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;

		if (FAILED(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error)))
			ED_LOG_ERROR("Failed to serialize root signature: {}", (char*)error->GetBufferPointer());

		if (FAILED(m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&internal_state->rootSignature))))
			ED_LOG_ERROR("Failed to create root signature");
	}

	GfxResult D3D12RHI::CreateBuffer(Buffer* buffer, BufferDesc* desc, const void* initial_data)
	{
		if (buffer == nullptr) return GfxResult::kInvalidParameter;
		if (desc   == nullptr) return GfxResult::kInvalidParameter;

		std::shared_ptr<D3D12Buffer> internal_state = std::make_shared<D3D12Buffer>();
		buffer->internal_state = internal_state;
		buffer->desc = *desc;

		buffer->size = buffer->desc.elementCount * buffer->desc.stride;
		if (buffer->desc.usage == BufferDesc::Uniform)
			buffer->size = ALIGN(buffer->size, 256);

		auto heapType = D3D12_HEAP_TYPE_UPLOAD;
		auto state = ResourceState::kRead;
		if (buffer->desc.usage == BufferDesc::Readback)
		{
			heapType = D3D12_HEAP_TYPE_READBACK;
			state = ResourceState::kCopyDest;
			ensureMsg(!initial_data, "A readback buffer cannot have initial data!");
			initial_data = nullptr;
		}

		D3D12MA::ALLOCATION_DESC allocationDesc = {};
		allocationDesc.HeapType = heapType;

		HRESULT hr = m_Allocator->CreateResource(&allocationDesc,
									&CD3DX12_RESOURCE_DESC::Buffer(buffer->size),
									Helpers::ConvertResourceState(state),
									nullptr,
									&internal_state->allocation,
									IID_PPV_ARGS(&internal_state->resource));

		if (hr != S_OK)
			return GfxResult::kOutOfMemory;

		internal_state->resource->Map(0, nullptr, &buffer->mappedData);
		if (initial_data)
		{
			memcpy(buffer->mappedData, initial_data, buffer->size);
			buffer->bIsInitialized = true;
		}

		switch (buffer->desc.usage)
		{
		case BufferDesc::Storage:
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = buffer->desc.elementCount;
			srvDesc.Buffer.StructureByteStride = buffer->desc.stride;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

			auto offset = AllocateHandle(&m_SRVHeap);
			internal_state->cpuHandle = GetCPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, offset);
			m_Device->CreateShaderResourceView(internal_state->resource.Get(), &srvDesc, internal_state->cpuHandle);
			internal_state->gpuHandle = GetGPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, offset);
		}
		break;
		case BufferDesc::Uniform:
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = internal_state->resource->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = buffer->size;

			auto offset = AllocateHandle(&m_SRVHeap);
			internal_state->cpuHandle = GetCPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, offset);
			m_Device->CreateConstantBufferView(&cbvDesc, internal_state->cpuHandle);
			internal_state->gpuHandle = GetGPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, offset);
		}
		break;
		}

		return GfxResult::kNoError;
	}

	void D3D12RHI::CreateAttachments(RenderPass* renderPass)
	{
		auto internal_state = ToInternal(renderPass);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(GetCPUHandle(&internal_state->rtvHeap));
		if (renderPass->desc.bIsSwapchainTarget)
		{
			// Create a RTV for each frame
			for (uint32_t i = 0; i < s_FrameCount; ++i)
			{
				Texture attachment;
				attachment.internal_state = std::make_shared<D3D12Texture>();
				attachment.imageFormat = Format::kRGBA8_UNORM;
				renderPass->colorAttachments.emplace_back(attachment);

				auto attachmentInternal_state = ToInternal(&attachment);

				if (FAILED(m_Swapchain->GetBuffer(i, IID_PPV_ARGS(&attachmentInternal_state->resource))))
					ensureMsg(false, "Failed to get render target from swapchain!");

				m_Device->CreateRenderTargetView(attachmentInternal_state->resource.Get(), nullptr, rtvHandle);
				rtvHandle.Offset(1, m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

				std::wstring rtName;
				Utils::StringConvert(renderPass->desc.debugName, rtName);;
				rtName += L"_backbuffer";
				attachmentInternal_state->resource->SetName(rtName.c_str());
				attachment.desc.width = renderPass->desc.width;
				attachment.desc.height = renderPass->desc.height;
			}
		}
		else
		{
			uint32_t attachmentIndex = 0;
			for (auto& format : renderPass->desc.attachmentsFormats)
			{
				if (IsDepthFormat(format))
					continue;

				size_t attachmentCount = renderPass->desc.attachmentsFormats.size();
				if (GetDepthFormatIndex(renderPass->desc.attachmentsFormats) > -1)
					attachmentCount = renderPass->desc.attachmentsFormats.size() - 1;

				auto createAttachmentResources = [&format, renderPass, this, &attachmentIndex, &rtvHandle](Texture* attachment, D3D12Texture* internal_state)
				{
					D3D12_RESOURCE_DESC resourceDesc = {};
					resourceDesc.Format = Helpers::ConvertFormat(format);
					resourceDesc.Width = renderPass->desc.width;
					resourceDesc.Height = renderPass->desc.height;
					resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
					resourceDesc.MipLevels = 1;
					resourceDesc.DepthOrArraySize = 1;
					resourceDesc.SampleDesc.Count = 1;
					resourceDesc.SampleDesc.Quality = 0;
					resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

					D3D12_CLEAR_VALUE clearValue = { Helpers::ConvertFormat(format), { 0.f, 0.f, 0.f, 0.f } };

					attachment->currentState = ResourceState::kPixelShader;

					D3D12MA::ALLOCATION_DESC allocationDesc = {};
					allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

					m_Allocator->CreateResource(&allocationDesc, &resourceDesc,
						Helpers::ConvertResourceState(attachment->currentState),
						&clearValue, &internal_state->allocation,
						IID_PPV_ARGS(&internal_state->resource));

					m_Device->CreateRenderTargetView(internal_state->resource.Get(), nullptr, rtvHandle);

					attachment->desc.width = renderPass->desc.width;
					attachment->desc.height = renderPass->desc.height;
					std::wstring rtName;
					Utils::StringConvert(renderPass->desc.debugName, rtName);
					rtName += L"_colorAttachment";
					internal_state->resource->SetName(rtName.c_str());
					internal_state->resource->SetName(rtName.c_str());
					m_Device->CreateShaderResourceView(internal_state->resource.Get(), nullptr, internal_state->cpuHandle);

					attachmentIndex++;
				};

				// If the amount of color attachments is equal to the amouth of formats, than its a resize operation
				if (renderPass->colorAttachments.size() == attachmentCount)
				{
					Texture* attachment = &renderPass->colorAttachments[attachmentIndex];
					D3D12Texture* internal_state = ToInternal(attachment);
					createAttachmentResources(attachment, internal_state);
				}
				else
				{
					auto offset = AllocateHandle(&m_SRVHeap);
					Texture attachment = {};
					attachment.internal_state = std::make_shared<D3D12Texture>();
					attachment.imageFormat = format;
					D3D12Texture* internal_state = ToInternal(&attachment);
					internal_state->cpuHandle = GetCPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, offset);
					internal_state->gpuHandle = GetGPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, offset);
					renderPass->colorAttachments.emplace_back(attachment);
					createAttachmentResources(&attachment, internal_state);
				}
			}
		}

		// Create DSV
		auto depthIndex = GetDepthFormatIndex(renderPass->desc.attachmentsFormats);
		if (depthIndex > -1)
		{
			Format depthFormat = renderPass->desc.attachmentsFormats[depthIndex];
			DXGI_FORMAT format, resourceFormat, srvFormat;
			if (depthFormat == Format::kDepth24Stencil8)
			{
				format = Helpers::ConvertFormat(depthFormat);
				resourceFormat = DXGI_FORMAT_R24G8_TYPELESS;
				srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			}
			else if (depthFormat == Format::kDepth32_FLOAT)
			{
				format = Helpers::ConvertFormat(depthFormat);
				resourceFormat = DXGI_FORMAT_R32_TYPELESS;
				srvFormat = DXGI_FORMAT_R32_FLOAT;
			}
			else if (depthFormat == Format::kDepth32FloatStencil8X24_UINT)
			{
				format = Helpers::ConvertFormat(depthFormat);
				resourceFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
				srvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
			}

			renderPass->depthStencil = {};
			auto dsvInternal_state = std::make_shared<D3D12Texture>();
			renderPass->depthStencil.internal_state = dsvInternal_state;
			renderPass->depthStencil.imageFormat = depthFormat;
			renderPass->depthStencil.currentState = ResourceState::kDepthWrite;

			D3D12_CLEAR_VALUE depthOptimizedClearValue;
			depthOptimizedClearValue.Format = format;
			depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
			depthOptimizedClearValue.DepthStencil.Stencil = 0;

			D3D12_RESOURCE_DESC resourceDesc = {};
			resourceDesc.Format = resourceFormat;
			resourceDesc.Width = renderPass->desc.width;
			resourceDesc.Height = renderPass->desc.height;
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourceDesc.MipLevels = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			D3D12MA::ALLOCATION_DESC allocationDesc = {};
			allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

			m_Allocator->CreateResource(&allocationDesc, &resourceDesc,
										Helpers::ConvertResourceState(renderPass->depthStencil.currentState),
										&depthOptimizedClearValue, &dsvInternal_state->allocation,
										IID_PPV_ARGS(&dsvInternal_state->resource));

			D3D12_DEPTH_STENCIL_VIEW_DESC depthStencil_desc = {};
			depthStencil_desc.Format = format;
			depthStencil_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			depthStencil_desc.Flags = D3D12_DSV_FLAG_NONE;

			auto dsvHandle = GetCPUHandle(&internal_state->dsvHeap);
			m_Device->CreateDepthStencilView(dsvInternal_state->resource.Get(), &depthStencil_desc, dsvHandle);

			auto srvOffset = AllocateHandle(&m_SRVHeap);
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = srvFormat;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			dsvInternal_state->cpuHandle = GetCPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvOffset);
			m_Device->CreateShaderResourceView(dsvInternal_state->resource.Get(), &srvDesc, dsvInternal_state->cpuHandle);
			dsvInternal_state->gpuHandle = GetGPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvOffset);

			std::wstring dsvName;
			Utils::StringConvert(renderPass->desc.debugName, dsvName);
			dsvName += L"_depthAttachment";
			dsvInternal_state->resource->SetName(dsvName.c_str());
			dsvInternal_state->allocation->SetName(dsvName.c_str());
		}
	}

	void D3D12RHI::CreateGraphicsPipeline(Pipeline* pipeline, PipelineDesc* desc)
	{
		// Get shader file path
		std::wstring shaderPath = L"shaders/";
		shaderPath.append(std::wstring(desc->programName.begin(), desc->programName.end()));
		shaderPath.append(L".hlsl");

		D3D12Pipeline* internal_state = ToInternal(pipeline);
		auto vertexShader = CompileShader(shaderPath, ShaderStage::kVS);
		internal_state->vertexReflection = vertexShader.reflection;
		auto pixel_shader = CompileShader(shaderPath, ShaderStage::kPS);
		internal_state->pixel_reflection = pixel_shader.reflection;

		ED_LOG_INFO("Compiled program '{}' successfully!", desc->programName);

		// Create the root signature
		CreateRootSignature(pipeline);

		// Get input elements from vertex shader reflection
		std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
		D3D12_SHADER_DESC shaderDesc;
		internal_state->vertexReflection->GetDesc(&shaderDesc);
		for (uint32_t i = 0; i < shaderDesc.InputParameters; ++i)
		{
			D3D12_SIGNATURE_PARAMETER_DESC desc = {};
			internal_state->vertexReflection->GetInputParameterDesc(i, &desc);

			uint32_t componentCount = 0;
			while (desc.Mask)
			{
				++componentCount;
				desc.Mask >>= 1;
			}

			DXGI_FORMAT componentFormat = DXGI_FORMAT_UNKNOWN;
			switch (desc.ComponentType)
			{
			case D3D_REGISTER_COMPONENT_UINT32:
				componentFormat = (componentCount == 1 ? DXGI_FORMAT_R32_UINT :
									componentCount == 2 ? DXGI_FORMAT_R32G32_UINT :
									componentCount == 3 ? DXGI_FORMAT_R32G32B32_UINT :
									DXGI_FORMAT_R32G32B32A32_UINT);
				break;
			case D3D_REGISTER_COMPONENT_SINT32:
				componentFormat = (componentCount == 1 ? DXGI_FORMAT_R32_SINT :
									componentCount == 2 ? DXGI_FORMAT_R32G32_SINT :
									componentCount == 3 ? DXGI_FORMAT_R32G32B32_SINT :
									DXGI_FORMAT_R32G32B32A32_SINT);
				break;
			case D3D_REGISTER_COMPONENT_FLOAT32:
				componentFormat = (componentCount == 1 ? DXGI_FORMAT_R32_FLOAT :
									componentCount == 2 ? DXGI_FORMAT_R32G32_FLOAT :
									componentCount == 3 ? DXGI_FORMAT_R32G32B32_FLOAT :
									DXGI_FORMAT_R32G32B32A32_FLOAT);
				break;
			default:
				break;  // D3D_REGISTER_COMPONENT_UNKNOWN
			}

			D3D12_INPUT_ELEMENT_DESC inputElement = {};
			inputElement.SemanticName = desc.SemanticName;
			inputElement.SemanticIndex = desc.SemanticIndex;
			inputElement.Format = componentFormat;
			inputElement.InputSlot = 0;
			inputElement.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
			inputElement.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
			inputElement.InstanceDataStepRate = 0;

			inputElements.emplace_back(inputElement);
		}

		D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
		renderTargetBlendDesc.BlendEnable = desc->bEnableBlending;
		renderTargetBlendDesc.LogicOpEnable = false;
		renderTargetBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		renderTargetBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		renderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
		renderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
		renderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		renderTargetBlendDesc.LogicOp = D3D12_LOGIC_OP_CLEAR;
		renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		// Describe and create pipeline state object(PSO)
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = internal_state->rootSignature.Get();
		psoDesc.VS = { vertexShader.blob->GetBufferPointer(), vertexShader.blob->GetBufferSize() };
		psoDesc.PS = { pixel_shader.blob->GetBufferPointer(), pixel_shader.blob->GetBufferSize() };
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = Helpers::ConvertCullMode(desc->cull_mode);
		psoDesc.RasterizerState.FrontCounterClockwise = desc->bIsFrontCounterClockwise;
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthFunc = Helpers::ConvertComparisonFunc(desc->depthFunc);
		psoDesc.DSVFormat = Helpers::ConvertFormat(desc->renderPass->depthStencil.imageFormat);
		psoDesc.InputLayout.NumElements = static_cast<uint32_t>(inputElements.size());
		psoDesc.InputLayout.pInputElementDescs = inputElements.data();
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.SampleDesc.Count = 1;

		uint32_t attachmentsIndex = 0;
		for (auto& attachment : desc->renderPass->colorAttachments)
		{
			psoDesc.RTVFormats[attachmentsIndex] = Helpers::ConvertFormat(attachment.imageFormat);
			psoDesc.BlendState.RenderTarget[0] = renderTargetBlendDesc;
			attachmentsIndex++;
		}
		attachmentsIndex++;
		psoDesc.NumRenderTargets = attachmentsIndex;

		if (FAILED(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&internal_state->pipelineState))))
			ensureMsg(false, "Failed to create graphics pipeline state!");

		std::wstring pipelineName;
		Utils::StringConvert(desc->programName, pipelineName);
		pipelineName.append(L"_PSO");
		internal_state->pipelineState->SetName(pipelineName.c_str());
		Utils::StringConvert(pipelineName, pipeline->debugName);
	}

	void D3D12RHI::CreateComputePipeline(Pipeline* pipeline, PipelineDesc* desc)
	{
		// Get shader file path
		std::wstring shaderPath = L"shaders/";
		shaderPath.append(std::wstring(desc->programName.begin(), desc->programName.end()));
		shaderPath.append(L".hlsl");

		ShaderResult computeShader = CompileShader(shaderPath, ShaderStage::kCS);
		D3D12Pipeline* internal_state = ToInternal(pipeline);
		internal_state->computeReflection = computeShader.reflection;

		ED_LOG_INFO("Compiled compute program '{}' successfully!", desc->programName);

		// Create the root signature
		CreateRootSignature(pipeline);

		// Describe and create pipeline state object(PSO)
		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = internal_state->rootSignature.Get();
		psoDesc.CS = { computeShader.blob->GetBufferPointer(), computeShader.blob->GetBufferSize() };
		if (FAILED(m_Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&internal_state->pipelineState))))
			ensureMsg(false, "Failed to create compute pipeline state!");

		std::wstring pipelineName;
		Utils::StringConvert(desc->programName, pipelineName);
		pipelineName.append(L"_PSO");
		internal_state->pipelineState->SetName(pipelineName.c_str());
		Utils::StringConvert(pipelineName, pipeline->debugName);
	}

	void D3D12RHI::CreateCommands()
	{
		// Describe and create graphics command queue
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		if (FAILED(m_Device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_CommandQueue))))
			ensureMsg(false, "Failed to create command queue!");

		// Create graphics command allocator
		if (FAILED(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator))))
			ensureMsg(false, "Failed to create command allocator!");
		m_CommandAllocator->SetName(L"gfxCommandAllocator");

		// Create graphics command list
		if (FAILED(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_CommandList))))
			ensureMsg(false, "Failed to create command list!");
		m_CommandList->SetName(L"gfxCommandList");
	}

	void D3D12RHI::EnsureMsgResourceState(Texture* resource, ResourceState destResourceState)
	{
		ensure(resource);

		if (resource->currentState != destResourceState)
		{
			ChangeResourceState(resource, resource->currentState, destResourceState);
			resource->currentState = destResourceState;
		}
	}

	GfxResult D3D12RHI::CreatePipeline(Pipeline* pipeline, PipelineDesc* desc)
	{
		if (pipeline == nullptr) return GfxResult::kInvalidParameter;
		if (desc     == nullptr) return GfxResult::kInvalidParameter;

		ED_PROFILE_FUNCTION();

		std::shared_ptr<D3D12Pipeline> internal_state = std::make_shared<D3D12Pipeline>();
		pipeline->internal_state = internal_state;
		pipeline->desc = *desc;

		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_DxcUtils));
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_DxcCompiler));

		// Create default include handler
		m_DxcUtils->CreateDefaultIncludeHandler(&m_DxcIncludeHandler);

		if (desc->type == kPipelineType_Graphics)
		{
			ensure(desc->renderPass);
			CreateGraphicsPipeline(pipeline, desc);
		}
		else if (desc->type == kPipelineType_Compute)
		{
			CreateComputePipeline(pipeline, desc);
		}

		return GfxResult::kNoError;
	}

	GfxResult D3D12RHI::CreateTexture(Texture* texture, std::string filePath, bool bGenerateMips)
	{
		ED_PROFILE_FUNCTION();

		TextureDesc desc = {};
		desc.bGenerateMips = bGenerateMips;

		int width, height, nchannels;
		void* textureFile;
		if (stbi_is_hdr(filePath.c_str()))
		{
			// Load texture from file
			textureFile = stbi_loadf(filePath.c_str(), &width, &height, &nchannels, 4);
			if (!textureFile)
			{
				ED_LOG_ERROR("Could not load texture file: {}", filePath);
				return GfxResult::kInvalidParameter;
			}

			desc.bIsSrgb = true;
		}
		else
		{
			// Load texture from file
			textureFile = stbi_load(filePath.c_str(), &width, &height, &nchannels, 4);
			if (!textureFile)
			{
				ED_LOG_ERROR("Could not load texture file: {}", filePath);
				return GfxResult::kInvalidParameter;
			}

		}
			
		desc.data = textureFile;
		desc.width = width;
		desc.height = height;
		desc.bIsStorage = false;

		return CreateTexture(texture, &desc);
	}

	GfxResult D3D12RHI::CreateTexture(Texture* texture, TextureDesc* desc)
	{
		ED_PROFILE_FUNCTION();

		ensureMsg(desc->type == TextureDesc::Texture2D, "Only texture2d is implemented!");

		std::shared_ptr<D3D12Texture> internal_state = std::make_shared<D3D12Texture>();
		texture->internal_state = internal_state;
		texture->desc = *desc;
		texture->imageFormat = texture->desc.bIsSrgb ? Format::kRGBA32_FLOAT : Format::kRGBA8_UNORM;
		texture->mipCount = desc->bGenerateMips ? CalculateMipCount(desc->width, desc->height) : 1;

		auto flags = D3D12_RESOURCE_FLAG_NONE;
		if (desc->bIsStorage || desc->bGenerateMips)
			flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		auto destResourceState = ResourceState::kPixelShader;
		if (desc->bIsStorage)
			destResourceState = ResourceState::kUnorderedAccess;
		texture->currentState = destResourceState;

		// Describe and create a texture2D
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = texture->mipCount;
		textureDesc.Format = Helpers::ConvertFormat(texture->imageFormat);
		textureDesc.Width = texture->desc.width;
		textureDesc.Height = texture->desc.height;
		textureDesc.Flags = flags;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		D3D12MA::ALLOCATION_DESC allocationDesc = {};
		allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

		HRESULT hr = m_Allocator->CreateResource(&allocationDesc,
									&textureDesc,
									Helpers::ConvertResourceState(destResourceState),
									nullptr,
									&internal_state->allocation,
									IID_PPV_ARGS(&internal_state->resource));

		if (hr != S_OK) return GfxResult::kOutOfMemory;

		if (desc->data)
		{
			EnsureMsgResourceState(texture, ResourceState::kCopyDest);
			SetTextureData(texture);
			if (desc->bGenerateMips)
				GenerateMips(texture);
			EnsureMsgResourceState(texture, destResourceState);
		}

		// Describe and create a the main SRV for the texture
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = Helpers::ConvertFormat(texture->imageFormat);
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = texture->mipCount;

		auto srvOffset = AllocateHandle(&m_SRVHeap);
		internal_state->cpuHandle = GetCPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvOffset);
		m_Device->CreateShaderResourceView(internal_state->resource.Get(), &srvDesc, internal_state->cpuHandle);
		internal_state->gpuHandle = GetGPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvOffset);

		if (desc->bIsStorage)
		{
			auto uavOffset = AllocateHandle(&m_SRVHeap);
			auto cpuHandle = GetCPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, uavOffset);
			m_Device->CreateUnorderedAccessView(internal_state->resource.Get(), nullptr, nullptr, cpuHandle);
		}

		return GfxResult::kNoError;
	}

	GfxResult D3D12RHI::CreateRenderPass(RenderPass* renderPass, RenderPassDesc* desc)
	{
		if (renderPass == nullptr) return GfxResult::kInvalidParameter;
		if (desc == nullptr)	   return GfxResult::kInvalidParameter;
		ensureMsg(desc->width > 0, "Render Pass width has to be > 0");
		ensureMsg(desc->height > 0, "Render Pass height has to be > 0");

		std::shared_ptr<D3D12RenderPass> internal_state = std::make_shared<D3D12RenderPass>();
		renderPass->internal_state = internal_state;
		renderPass->desc = *desc;
		renderPass->debugName = renderPass->desc.debugName;

		CreateDescriptorHeap(&internal_state->rtvHeap, s_FrameCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
		CreateDescriptorHeap(&internal_state->dsvHeap, 1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

		ensureMsg(desc->attachmentsFormats.size() > 0, "Can't create render pass without attachments");
		ensureMsg(desc->attachmentsFormats.size() < 8, "Can't create a render pass with more than 8 attachments");

		// NOTE: For now it is only possible to have one depth attachment
		CreateAttachments(renderPass);

		return GfxResult::kNoError;
	}

	GfxResult D3D12RHI::CreateGPUTimer(GPUTimer* timer)
	{
		if (timer == nullptr) return GfxResult::kInvalidParameter;

		std::shared_ptr<D3D12GPUTimer> internal_state = std::make_shared<D3D12GPUTimer>();
		timer->internal_state = internal_state;

		D3D12_QUERY_HEAP_DESC heapDesc = { };
		heapDesc.Count = 2;
		heapDesc.NodeMask = 0;
		heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		HRESULT hr = m_Device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&internal_state->queryHeap));
		if (hr != S_OK) return GfxResult::kInternalError;

		BufferDesc readbackDesc = {};
		readbackDesc.elementCount = 2;
		readbackDesc.stride = sizeof(uint64_t);
		readbackDesc.usage = BufferDesc::Readback;
		CreateBuffer(&timer->readbackBuffer, &readbackDesc, nullptr);

		return GfxResult::kNoError;
	}

	void D3D12RHI::SetName(Resource* child, std::string& name)
	{
		ensure(child);

		child->debugName = name;
		std::wstring wname;
		Utils::StringConvert(name, wname);
		auto internal_state = ToInternal(child);
		internal_state->resource->SetName(wname.c_str());
	}

	void D3D12RHI::BeginGPUTimer(GPUTimer* timer)
	{
		ensure(timer);

		auto internal_state = ToInternal(timer);
		m_CommandList->EndQuery(internal_state->queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
	}

	void D3D12RHI::EndGPUTimer(GPUTimer* timer)
	{
		ensure(timer);

		auto internal_state = ToInternal(timer);
		auto readbackInternal_state = ToInternal(&timer->readbackBuffer);

		uint64_t gpuFrequency;
		m_CommandQueue->GetTimestampFrequency(&gpuFrequency);

		// Insert the end timestamp
		m_CommandList->EndQuery(internal_state->queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);

		// Resolve the data
		m_CommandList->ResolveQueryData(internal_state->queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, readbackInternal_state->resource.Get(), 0);

		// Get the query data
		uint64_t* queryData = reinterpret_cast<uint64_t*>(timer->readbackBuffer.mappedData);
		uint64_t startTime = queryData[0];
		uint64_t endTime = queryData[1];

		if (endTime > startTime)
		{
			uint64_t delta = endTime - startTime;
			double frequency = double(gpuFrequency);
			timer->elapsedTime = (delta / frequency) * 1000.0;
		}
	}

	GfxResult D3D12RHI::UpdateBufferData(Buffer* buffer, const void* data, uint32_t count)
	{
		if (buffer == nullptr) return GfxResult::kInvalidParameter;
		if (data == nullptr)   return GfxResult::kInvalidParameter;

		if (count > 0)
		{
			buffer->desc.elementCount = count;
			buffer->size = buffer->desc.elementCount * buffer->desc.stride;
		}
		memcpy(buffer->mappedData, data, buffer->size);

		return GfxResult::kNoError;
	}

	void D3D12RHI::GenerateMips(Texture* texture)
	{
		ensure(texture);
		auto internal_state = ToInternal(texture);

		//Prepare the shader resource view description for the source texture
		D3D12_SHADER_RESOURCE_VIEW_DESC srcTextureSrvDesc = {};
		srcTextureSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srcTextureSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

		//Prepare the unordered access view description for the destination texture
		D3D12_UNORDERED_ACCESS_VIEW_DESC destTextureUavDesc = {};
		destTextureUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		BindPipeline(&m_MipsPipeline);
		SetDescriptorHeap(&m_SRVHeap);
		EnsureMsgResourceState(texture, ResourceState::kUnorderedAccess);
		for (uint32_t i = 0; i < texture->mipCount - 1; ++i)
		{
			// Update source texture subresource state
			uint32_t srcTexture = D3D12CalcSubresource(i, 0, 0, texture->mipCount, 1);
			ChangeResourceState(texture, ResourceState::kUnorderedAccess, ResourceState::kNonPixelShader, (int)srcTexture);

			//Get mipmap dimensions
			uint32_t width = std::max<uint32_t>(texture->desc.width >> (i + 1), 1);
			uint32_t height = std::max<uint32_t>(texture->desc.height >> (i + 1), 1);
			glm::vec2 texel_size = { 1.0f / width, 1.0f / height };

			//Create shader resource view for the source texture in the descriptor heap
			{
				auto srvOffset = AllocateHandle(&m_SRVHeap);
				srcTextureSrvDesc.Format = Helpers::ConvertFormat(texture->imageFormat);
				srcTextureSrvDesc.Texture2D.MipLevels = 1;
				srcTextureSrvDesc.Texture2D.MostDetailedMip = i;
				m_Device->CreateShaderResourceView(internal_state->resource.Get(), &srcTextureSrvDesc, GetCPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvOffset));
				BindRootParameter("SrcTexture", GetGPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srvOffset));
			}

			//Create unordered access view for the destination texture in the descriptor heap
			{
				auto uavOffset = AllocateHandle(&m_SRVHeap);
				destTextureUavDesc.Format = Helpers::ConvertFormat(texture->imageFormat);
				destTextureUavDesc.Texture2D.MipSlice = i + 1;
				m_Device->CreateUnorderedAccessView(internal_state->resource.Get(), nullptr, &destTextureUavDesc, GetCPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, uavOffset));
				BindRootParameter("DstTexture", GetGPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, uavOffset));
			}

			//Pass the destination texture pixel size to the shader as constants
			BindParameter("TexelSize", &texel_size, sizeof(glm::vec2));

			m_CommandList->Dispatch(std::max<uint32_t>(width / 8, 1u), std::max<uint32_t>(height / 8, 1u), 1);
			
			//Wait for all accesses to the destination texture UAV to be finished before generating the next mipmap, as it will be the source texture for the next mipmap
			m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(internal_state->resource.Get()));

			ChangeResourceState(texture, ResourceState::kNonPixelShader, ResourceState::kUnorderedAccess, (int)srcTexture);
		}
	}

	uint64_t D3D12RHI::GetTextureID(Texture* texture)
	{
		ensure(texture);

		auto internal_state = ToInternal(texture);
		return internal_state->gpuHandle.ptr;
	}

	GfxResult D3D12RHI::ReloadPipeline(Pipeline* pipeline)
	{
		if (pipeline == nullptr) return GfxResult::kInvalidParameter;

		CreatePipeline(pipeline, &pipeline->desc);

		return GfxResult::kNoError;
	}

	void D3D12RHI::EnableImGui()
	{
		// Setup imgui
		auto offset = AllocateHandle(&m_SRVHeap);
		ImGui_ImplDX12_Init(m_Device.Get(), s_FrameCount, DXGI_FORMAT_R8G8B8A8_UNORM, m_SRVHeap.heap.Get(),
							GetCPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, offset), GetGPUHandle(&m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, offset));
		m_bIsImguiInitialized = true;

		ImGuiNewFrame();
	}

	void D3D12RHI::ImGuiNewFrame()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void D3D12RHI::BindPipeline(Pipeline* pipeline)
	{
		ensure(pipeline);

		auto internal_state = ToInternal(pipeline);

		if (pipeline->desc.type == kPipelineType_Graphics)
			m_CommandList->SetGraphicsRootSignature(internal_state->rootSignature.Get());
		else if (pipeline->desc.type == kPipelineType_Compute)
			m_CommandList->SetComputeRootSignature(internal_state->rootSignature.Get());
		
		m_CommandList->SetPipelineState(internal_state->pipelineState.Get());
		m_BoundPipeline = pipeline;
	}

	void D3D12RHI::BindVertexBuffer(Buffer* vertexBuffer)
	{
		ensure(vertexBuffer);

		auto internal_state = ToInternal(vertexBuffer);
		ensureMsg(internal_state->resource != nullptr, "Can't bind a empty vertex buffer!");
		ensureMsg(m_BoundPipeline->desc.type == kPipelineType_Graphics, "Can't bind a vertex buffer on a compute pipeline!");

		m_BoundVertexBuffer.BufferLocation	= internal_state->resource->GetGPUVirtualAddress();
		m_BoundVertexBuffer.SizeInBytes		= vertexBuffer->size;
		m_BoundVertexBuffer.StrideInBytes	= vertexBuffer->desc.stride;
	}

	void D3D12RHI::BindIndexBuffer(Buffer* indexBuffer)
	{
		ensure(indexBuffer);

		auto internal_state = ToInternal(indexBuffer);
		ensureMsg(internal_state->resource != nullptr, "Can't bind a empty index buffer!");
		ensureMsg(m_BoundPipeline->desc.type == kPipelineType_Graphics, "Can't bind a index buffer on a compute pipeline!");

		m_BoundIndexBuffer.BufferLocation	= internal_state->resource->GetGPUVirtualAddress();
		m_BoundIndexBuffer.SizeInBytes		= indexBuffer->size;
		m_BoundIndexBuffer.Format			= DXGI_FORMAT_R32_UINT;
	}

	void D3D12RHI::BindParameter(const std::string& parameterName, Buffer* buffer)
	{
		ensure(buffer);

		auto internal_state = ToInternal(buffer);
		BindRootParameter(parameterName, internal_state->gpuHandle);
	}

	void D3D12RHI::BindParameter(const std::string& parameterName, Texture* texture, TextureUsage usage)
	{
		ensure(texture);

		auto internal_state = ToInternal(texture);

		auto gpuHandle = internal_state->gpuHandle;
		if (usage == kReadWrite)
		{
			gpuHandle.ptr += static_cast<uint64_t>(m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

			EnsureMsgResourceState(texture, ResourceState::kUnorderedAccess);
		}
		else
		{
			EnsureMsgResourceState(texture, ResourceState::kPixelShader);
		}
		BindRootParameter(parameterName, gpuHandle);
	}

	void D3D12RHI::BindParameter(const std::string& parameterName, void* data, size_t size)
	{
		ensure(data);

		uint32_t rootParameterIndex = GetRootParameterIndex(parameterName);
		uint32_t num_values = static_cast<uint32_t>(size / sizeof(uint32_t));
		if (m_BoundPipeline->desc.type == kPipelineType_Graphics)
			m_CommandList->SetGraphicsRoot32BitConstants(rootParameterIndex, num_values, data, 0);
		else if (m_BoundPipeline->desc.type == kPipelineType_Compute)
			m_CommandList->SetComputeRoot32BitConstants(rootParameterIndex, num_values, data, 0);
	}

	void D3D12RHI::BindRootParameter(const std::string& parameterName, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
	{
		uint32_t rootParameterIndex = GetRootParameterIndex(parameterName);
		if (m_BoundPipeline->desc.type == kPipelineType_Graphics)
			m_CommandList->SetGraphicsRootDescriptorTable(rootParameterIndex, gpuHandle);
		else if (m_BoundPipeline->desc.type == kPipelineType_Compute)
			m_CommandList->SetComputeRootDescriptorTable(rootParameterIndex, gpuHandle);
	}

	void D3D12RHI::SetTextureData(Texture* texture)
	{
		auto internal_state = ToInternal(texture);
		uint32_t dataCount = texture->mipCount;

		ComPtr<ID3D12Resource> textureUploadHeap;
		D3D12MA::Allocation* uploadAllocation;
		const uint64_t uploadBufferSize = GetRequiredIntermediateSize(internal_state->resource.Get(), 0, dataCount);

		// Create the GPU upload buffer
		D3D12MA::ALLOCATION_DESC uploadAllocationDesc = {};
		uploadAllocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

		m_Allocator->CreateResource(&uploadAllocationDesc,
									&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
									D3D12_RESOURCE_STATE_GENERIC_READ,
									nullptr,
									&uploadAllocation,
									IID_PPV_ARGS(&textureUploadHeap));

		// Copy data to the intermediate upload heap and then schedule a copy 
		// from the upload heap to the Texture2D.
		std::vector<D3D12_SUBRESOURCE_DATA> data(dataCount);
		
		uint32_t width = texture->desc.width;
		uint32_t height = texture->desc.height;
		for (uint32_t i = 0; i < dataCount; ++i)
		{
			D3D12_SUBRESOURCE_DATA textureData = {};
			textureData.pData = texture->desc.data;
			textureData.RowPitch = width * 4;
			if (texture->desc.bIsSrgb)
				textureData.RowPitch *= sizeof(float);
			textureData.SlicePitch = height * textureData.RowPitch;
			data[i] = textureData;

			width = std::max<uint32_t>(width / 2, 1u);
			height = std::max<uint32_t>(height / 2, 1u);
		}

		UpdateSubresources(m_CommandList.Get(), internal_state->resource.Get(), textureUploadHeap.Get(), 0, 0, dataCount, data.data());

		m_CommandList->Close();
		ID3D12CommandList* commandLists[] = { m_CommandList.Get() };
		m_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
		m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);

		WaitForGPU();

		uploadAllocation->Release();
	}

	void D3D12RHI::BeginRender()
	{
		// Set SRV Descriptor heap
		SetDescriptorHeap(&m_SRVHeap);
	}

	void D3D12RHI::BeginRenderPass(RenderPass* renderPass)
	{
		ensure(renderPass);

		auto internal_state = ToInternal(renderPass);

		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR(0, 0, 0), renderPass->debugName.c_str());

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetCPUHandle(&internal_state->dsvHeap, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 0);
		if (renderPass->desc.bIsSwapchainTarget)
		{
			rtvHandle = GetCPUHandle(&internal_state->rtvHeap, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_FrameIndex);
			EnsureMsgResourceState(&renderPass->colorAttachments[m_FrameIndex], ResourceState::kRenderTarget);
		}
		else
		{
			for (auto& attachment : renderPass->colorAttachments)
			{
				if (renderPass->desc.width != attachment.desc.width ||
					renderPass->desc.height != attachment.desc.height)
				{
					CreateAttachments(renderPass);
				}
				rtvHandle = GetCPUHandle(&internal_state->rtvHeap, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 0);
				EnsureMsgResourceState(&attachment, ResourceState::kRenderTarget);
			}
		}

		float* clearColor = (float*)&renderPass->desc.clearColor;
		CD3DX12_CLEAR_VALUE clearValue{ DXGI_FORMAT_R32G32B32_FLOAT, clearColor };

		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE beginAccessType = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
		D3D12_RENDER_PASS_ENDING_ACCESS_TYPE endAccessType = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

		// RTV
		D3D12_RENDER_PASS_BEGINNING_ACCESS rtvBeginAccess{ beginAccessType, { clearValue } };
		D3D12_RENDER_PASS_ENDING_ACCESS rtvEndAccess{ endAccessType, {} };
		internal_state->rtvDesc = { rtvHandle, rtvBeginAccess, rtvEndAccess };

		// DSV
		D3D12_RENDER_PASS_BEGINNING_ACCESS_CLEAR_PARAMETERS dsvClear = {};
		dsvClear.ClearValue.Format = Helpers::ConvertFormat(renderPass->depthStencil.imageFormat);
		dsvClear.ClearValue.DepthStencil.Depth = 1.0f;
		D3D12_RENDER_PASS_BEGINNING_ACCESS dsvBeginAccess{ beginAccessType, { dsvClear } };
		D3D12_RENDER_PASS_ENDING_ACCESS dsvEndAccess{ endAccessType, {} };
		internal_state->dsvDesc = { dsvHandle, dsvBeginAccess, dsvBeginAccess, dsvEndAccess, dsvEndAccess };

		m_CommandList->BeginRenderPass(1, &internal_state->rtvDesc, &internal_state->dsvDesc, D3D12_RENDER_PASS_FLAG_NONE);
	}

	void D3D12RHI::SetSwapchainTarget(RenderPass* renderPass)
	{
		ensure(renderPass);

		ensureMsg(renderPass->desc.bIsSwapchainTarget, "SetSwapchainTarget render pass was not created as a swapchain target!");
		m_SwapchainTarget = renderPass;
	}

	void D3D12RHI::EndRenderPass(RenderPass* renderPass)
	{
		ensure(renderPass);

		if (m_bIsImguiInitialized && renderPass->desc.bIsImguiPass)
		{
			ImGui::Render();
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_CommandList.Get());

			// Update and Render additional Platform Windows
			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
			}
		}

		m_CommandList->EndRenderPass();
		PIXEndEvent(m_CommandList.Get());

		if (renderPass->desc.bIsSwapchainTarget)
		{
			EnsureMsgResourceState(&renderPass->colorAttachments[m_FrameIndex], ResourceState::kPresent);
		}
		else
		{
			for (auto& attachment : renderPass->colorAttachments)
				EnsureMsgResourceState(&attachment, ResourceState::kPixelShader);
		}
	}

	void D3D12RHI::EndRender()
	{
		if (m_bIsImguiInitialized)
			ImGuiNewFrame();
	}

	void D3D12RHI::PrepareDraw()
	{
		auto internal_state = ToInternal(m_BoundPipeline);

		ensureMsg(internal_state->pipelineState != nullptr, "Can't Draw without a valid pipeline bound!");
		ensureMsg(internal_state->rootSignature != nullptr, "Can't Draw without a valid pipeline bound!");

		ED_PROFILE_FUNCTION();

		ED_PROFILE_GPU_CONTEXT(m_CommandList.Get());
		ED_PROFILE_GPU_FUNCTION("D3D12RHI::PrepareDraw");

		D3D12_VIEWPORT viewport;
		viewport.Width = static_cast<float>(m_BoundPipeline->desc.renderPass->desc.width);
		viewport.Height = static_cast<float>(m_BoundPipeline->desc.renderPass->desc.height);
		viewport.MaxDepth = 1.0f;
		viewport.MinDepth = m_BoundPipeline->desc.minDepth;
		viewport.TopLeftX = 0.0;
		viewport.TopLeftY = 0.0;
		m_CommandList->RSSetViewports(1, &viewport);
		m_CommandList->RSSetScissorRects(1, &m_Scissor);

		m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	void D3D12RHI::Draw(uint32_t vertexCount, uint32_t instanceCount /*= 1*/, uint32_t startVertexLocation /*= 0*/, uint32_t startInstanceLocation /*= 0*/)
	{
		PrepareDraw();

		m_CommandList->IASetVertexBuffers(0, 1, &m_BoundVertexBuffer);
		m_CommandList->DrawInstanced(vertexCount, instanceCount, startVertexLocation, startInstanceLocation);
	}

	void D3D12RHI::DrawIndexed(uint32_t indexCount, uint32_t instanceCount /*= 1*/, uint32_t startIndexLocation /*= 0*/, uint32_t baseVertexLocation /*= 0*/, uint32_t startInstanceLocation /*= 0*/)
	{
		PrepareDraw();

		m_CommandList->IASetVertexBuffers(0, 1, &m_BoundVertexBuffer);
		m_CommandList->IASetIndexBuffer(&m_BoundIndexBuffer);
		m_CommandList->DrawIndexedInstanced(indexCount, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
	}

	void D3D12RHI::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
	{
		m_CommandList->Dispatch(groupCountX, groupCountY, groupCountZ);
	}

	void D3D12RHI::GetHardwareAdapter()
	{
		m_Adapter = nullptr;

		ComPtr<IDXGIAdapter1> adapter;

		// Get IDXGIFactory6
		ComPtr<IDXGIFactory6> factory6;
		if (!FAILED(m_Factory->QueryInterface(IID_PPV_ARGS(&factory6))))
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
				if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), s_FeatureLevel, _uuidof(ID3D12Device), nullptr)))
					break;
			}
		}
		else
		{
			ensureMsg(false, "Failed to get IDXGIFactory6!");
		}

		ensureMsg(adapter.Get() != nullptr, "Failed to get a compatible adapter!");

		 adapter.Swap(m_Adapter);
	}

	void D3D12RHI::Render()
	{
		ED_PROFILE_FUNCTION();
		ED_PROFILE_GPU_CONTEXT(m_CommandList.Get());
		ED_PROFILE_GPU_FUNCTION("D3D12RHI::Render");

		if (FAILED(m_CommandList->Close()))
			ensureMsg(false, "Failed to close command list");

		ID3D12CommandList* commandLists[] = { m_CommandList.Get() };
		m_CommandQueue->ExecuteCommandLists(ARRAYSIZE(commandLists), commandLists);

		// Schedule a Signal command in the queue
		const uint64_t currentFenceValue = m_FenceValues[m_FrameIndex];
		m_CommandQueue->Signal(m_Fence.Get(), currentFenceValue);

		ED_PROFILE_GPU_FLIP(m_Swapchain.Get());
		m_Swapchain->Present(1, 0);

		// Update the frame index
		m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

		// If the next frame is not ready to be rendered yet, wait until it is ready
		if (m_Fence->GetCompletedValue() < m_FenceValues[m_FrameIndex])
		{
			m_Fence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_FenceEvent);
			WaitForSingleObject(m_FenceEvent, INFINITE); // wait for GPU to complete
		}

		// Set the fence value for the next frame
		m_FenceValues[m_FrameIndex] = currentFenceValue + 1;

		WaitForGPU();

		m_CommandAllocator->Reset();

		m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);
	}

	GfxResult D3D12RHI::Resize(uint32_t width, uint32_t height)
	{
		ensureMsg(m_SwapchainTarget, "Failed to resize, no Render Target Render Pass assigned");

		auto internal_state = ToInternal(m_SwapchainTarget);
		if (m_Device && m_Swapchain)
		{
			ED_LOG_TRACE("Resizing Window to {}x{}", width, height);

			WaitForGPU();
			if (FAILED(m_CommandList->Close()))
			{
				ensureMsg(false, "Failed to close command list");
				return GfxResult::kInternalError;
			}

			for (size_t i = 0; i < s_FrameCount; ++i)
			{
				auto rtInternal_state = ToInternal(&m_SwapchainTarget->colorAttachments[i]);
				rtInternal_state->resource.Reset();
				m_FenceValues[i] = m_FenceValues[m_FrameIndex];
			}
			
			m_SwapchainTarget->depthStencil = {};

			m_Swapchain->ResizeBuffers(s_FrameCount, width, height, DXGI_FORMAT_UNKNOWN, 0);

			m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

			m_SwapchainTarget->colorAttachments.clear();
			m_SwapchainTarget->desc.width = width;
			m_SwapchainTarget->desc.height = height;
			CreateAttachments(m_SwapchainTarget);
			
			m_Scissor  = CD3DX12_RECT(0, 0, width, height);

			m_CommandAllocator->Reset();
			m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);
		}

		return GfxResult::kNoError;
	}

	void D3D12RHI::ReadPixelFromTexture(uint32_t x, uint32_t y, Texture* texture, glm::vec4& pixel)
	{
		ensure(texture);

		auto internal_state = ToInternal(texture);

		ComPtr<ID3D12Resource> stagingBuffer;
		ComPtr<D3D12MA::Allocation> stagingAllocation;
		
		uint64_t width = texture->desc.width * 32;
		width = ALIGN(width, 256);

		uint64_t stagingBufferSize = width * texture->desc.height;

		D3D12MA::ALLOCATION_DESC allocationDesc = {};
		allocationDesc.HeapType = D3D12_HEAP_TYPE_READBACK;

		HRESULT hr = m_Allocator->CreateResource(&allocationDesc,
									&CD3DX12_RESOURCE_DESC::Buffer(stagingBufferSize),
									D3D12_RESOURCE_STATE_COPY_DEST,
									nullptr,
									&stagingAllocation,
									IID_PPV_ARGS(&stagingBuffer));
		ensure(hr == S_OK);

		D3D12_SUBRESOURCE_FOOTPRINT subresourceFootprint = {};
		subresourceFootprint.Width = texture->desc.width;
		subresourceFootprint.Height = texture->desc.height;
		subresourceFootprint.Depth = 1;
		subresourceFootprint.Format = Helpers::ConvertFormat(texture->imageFormat);
		subresourceFootprint.RowPitch = static_cast<uint32_t>(width);

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT textureFootprint = {};
		textureFootprint.Footprint = subresourceFootprint;
		textureFootprint.Offset = 0;

		D3D12_TEXTURE_COPY_LOCATION src = {};
		src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src.SubresourceIndex = 0;
		src.pResource = internal_state->resource.Get();

		D3D12_TEXTURE_COPY_LOCATION dst = {};
		dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dst.PlacedFootprint = textureFootprint;
		dst.pResource = stagingBuffer.Get();

		D3D12_BOX srcRegion = {};
		srcRegion.top = y;
		srcRegion.left = x;
		srcRegion.right = texture->desc.width;
		srcRegion.bottom = texture->desc.height;
		srcRegion.back = 1;
		srcRegion.front = 0;

		EnsureMsgResourceState(texture, ResourceState::kCopySrc);
		m_CommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, &srcRegion);
		EnsureMsgResourceState(texture, ResourceState::kPixelShader);

		m_CommandList->Close();
		ID3D12CommandList* commandLists[] = { m_CommandList.Get() };
		m_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
		m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);

		WaitForGPU();

		void* pixel_data;
		stagingBuffer->Map(0, nullptr, &pixel_data);
		pixel = *(glm::vec4*)pixel_data;
	}

	void D3D12RHI::ChangeResourceState(Texture* resource, ResourceState currentState, ResourceState desiredState, int subresource /*= -1*/)
	{
		ensure(resource);

		auto internal_state = ToInternal(resource);
		uint32_t sub = subresource == -1 ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : (uint32_t)subresource;

		m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
												internal_state->resource.Get(), 
												Helpers::ConvertResourceState(currentState),
												Helpers::ConvertResourceState(desiredState),
												sub));
	}

	D3D12_GPU_DESCRIPTOR_HANDLE D3D12RHI::GetGPUHandle(DescriptorHeap* descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType /*= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV*/, int32_t offset /*= 0*/)
	{
		CD3DX12_GPU_DESCRIPTOR_HANDLE handle(descriptorHeap->heap->GetGPUDescriptorHandleForHeapStart());
		handle.Offset(offset, m_Device->GetDescriptorHandleIncrementSize(heapType));

		return handle;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12RHI::GetCPUHandle(DescriptorHeap* descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType /*= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV*/, int32_t offset /*= 0*/)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(descriptorHeap->heap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(offset, m_Device->GetDescriptorHandleIncrementSize(heapType));

		return handle;
	}

	uint32_t D3D12RHI::AllocateHandle(DescriptorHeap* descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE heapType /*= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV*/)
	{
		if (heapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
			ensureMsg(descriptorHeap->offset < s_SRVDescriptorCount, "This SRV Descriptor heap is full!");

		auto handleOffset = descriptorHeap->offset;
		descriptorHeap->offset++;
		return handleOffset;
	}

	GfxResult D3D12RHI::CreateDescriptorHeap(DescriptorHeap* descriptorHeap, uint32_t num_descriptors, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType, D3D12_DESCRIPTOR_HEAP_FLAGS flags /*= D3D12_DESCRIPTOR_HEAP_FLAG_NONE*/)
	{
		if (descriptorHeap == nullptr) return GfxResult::kInvalidParameter;

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = num_descriptors;
		heapDesc.Type = descriptorType;
		heapDesc.Flags = flags;

		if (FAILED(m_Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap->heap))))
		{
			ensureMsg(false, "Failed to create Shader Resource View descriptor heap!");
			return GfxResult::kInternalError;
		}

		return GfxResult::kNoError;
	}

	void D3D12RHI::SetDescriptorHeap(DescriptorHeap* descriptorHeap)
	{
		// Right now this is ok to use because we are not using more than one desriptor heap
		ID3D12DescriptorHeap* heaps[] = { descriptorHeap->heap.Get() };
		m_CommandList->SetDescriptorHeaps(_countof(heaps), heaps);
	}

	void D3D12RHI::WaitForGPU()
	{
		// Schedule a signal command in the queue
		m_CommandQueue->Signal(m_Fence.Get(), m_FenceValues[m_FrameIndex]);

		// Wait until the fence has been processed
		m_Fence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_FenceEvent);
		WaitForSingleObject(m_FenceEvent, INFINITE);

		// Increment the fence value for the current frame
		m_FenceValues[m_FrameIndex]++;
	}

	uint32_t D3D12RHI::GetRootParameterIndex(const std::string& parameterName)
	{
		auto rootParameterIndex = m_BoundPipeline->rootParameterIndices.find(parameterName.data());

		bool bWasRootParameterFound = rootParameterIndex != m_BoundPipeline->rootParameterIndices.end();
		ensureMsg(bWasRootParameterFound, "Failed to find root parameter!");

		return rootParameterIndex->second;
	}
}
