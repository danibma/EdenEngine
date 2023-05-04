#include "D3D12DynamicRHI.h"

#include <cstdint>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <imgui/backends/imgui_impl_dx12.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/ImGuizmo.h>

#include <dxgidebug.h>
#include <dxgi1_6.h>

#include <WinPixEventRuntime/pix3.h>

#include "Utilities/Utils.h"
#include "D3D12DescriptorHeap.h"

// From Guillaume Boisse "gfx" https://github.com/gboisse/gfx/blob/b83878e562c2c205000b19c99cf24b13973dedb2/gfx_core.h#L77
#define ALIGN(VAL, ALIGN)   \
    (((VAL) + (static_cast<decltype(VAL)>(ALIGN) - 1)) & ~(static_cast<decltype(VAL)>(ALIGN) - 1))

namespace Eden
{
	GfxResult D3D12DynamicRHI::Init(Window* window)
	{
		m_Scissor = CD3DX12_RECT(0, 0, window->GetWidth(), window->GetHeight());
		m_CurrentAPI = kApi_D3D12;

		m_Device = enew D3D12Device();

		// Create commands
		CreateCommands();

		// Describe and create the swap chain
		DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
		swapchainDesc.Width = window->GetWidth();
		swapchainDesc.Height = window->GetHeight();
		swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchainDesc.SampleDesc.Count = 1;
		swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainDesc.BufferCount = GFrameCount;
		swapchainDesc.Scaling = DXGI_SCALING_NONE;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		if (!m_VSyncEnabled)
			swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

		ComPtr<IDXGISwapChain1> swapchain;
		if (FAILED(m_Device->GetFactory()->CreateSwapChainForHwnd(m_CommandQueue.Get(), window->GetHandle(), &swapchainDesc, nullptr, nullptr, &swapchain))) 
		{
			ensureMsg(false, "Failed to create swapchain!");
			return GfxResult::kInternalError;
		}

		// NOTE(Daniel): Disable fullscreen
		m_Device->GetFactory()->MakeWindowAssociation(window->GetHandle(), DXGI_MWA_NO_ALT_ENTER);

		swapchain.As(&m_Swapchain);
		m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

		// Create synchronization objects
		{
			if (FAILED(m_Device->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence))))
			{
				ensureMsg(false, "Failed to create fence!");
				return GfxResult::kInternalError;
			}
			m_FenceValues[m_FrameIndex]++;

			// Create an event handler to use for frame synchronization
			m_FenceEvent = CreateEvent(nullptr, false, false, nullptr);
			ensureMsg(m_FenceEvent != nullptr, Utils::GetLastErrorMessage());
		}

		// Associate the graphics device Resize with the window resize callback
		window->SetResizeCallback([&](uint32_t x, uint32_t y) 
		{ 
			GfxResult error = Resize(x, y); 
			ensure(error == GfxResult::kNoError);
		});

		// Create mips pipeline
		PipelineDesc mipsDesc = {};
		mipsDesc.programName = "GenerateMips";
		mipsDesc.type = kPipelineType_Compute;
		m_MipsPipeline = CreatePipeline(&mipsDesc);

		return GfxResult::kNoError;
	}

	void D3D12DynamicRHI::Shutdown()
	{
		WaitForGPU();

		if (m_bIsImguiInitialized)
		{
			ImGui_ImplDX12_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
		}

		CloseHandle(m_FenceEvent);

		m_BoundPipeline.Reset();
		m_SwapchainTarget.Reset();
		m_MipsPipeline.Reset();

		edelete m_Device;
	}

	D3D12DynamicRHI::ShaderResult D3D12DynamicRHI::CompileShader(std::filesystem::path filePath, ShaderStage stage)
	{
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

	D3D12_STATIC_SAMPLER_DESC D3D12DynamicRHI::CreateSamplerDesc(uint32_t shaderRegister, uint32_t registerSpace, D3D12_SHADER_VISIBILITY shaderVisibility, D3D12_TEXTURE_ADDRESS_MODE addressMode)
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

	void D3D12DynamicRHI::CreateRootSignature(PipelineRef pipeline)
	{
		D3D12Pipeline* dxPipeline = static_cast<D3D12Pipeline*>(pipeline.Get());

		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (!m_Device->SupportsRootSignature1_1())
		{
			ED_LOG_WARN("Root signature version 1.1 not available, switching to 1.0");
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		uint32_t srvCount = 0;
		std::vector<D3D12_ROOT_PARAMETER1> rootParameters;
		std::vector<D3D12_DESCRIPTOR_RANGE1> descriptorRanges;
		std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;
		
		std::vector<uint32_t> shaderCounts;
		if (dxPipeline->vertexReflection)
			shaderCounts.emplace_back(uint32_t(ShaderStage::kVS));
		if (dxPipeline->pixel_reflection)
			shaderCounts.emplace_back(uint32_t(ShaderStage::kPS));
		if (dxPipeline->computeReflection)
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
				reflection = dxPipeline->vertexReflection;
				shaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
				break;
			case ShaderStage::kPS:
				reflection = dxPipeline->pixel_reflection;
				shaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
				break;
			case ShaderStage::kCS:
				reflection = dxPipeline->computeReflection;
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

		DX_CHECK(m_Device->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&dxPipeline->rootSignature)));
	}

	BufferRef D3D12DynamicRHI::CreateBuffer(BufferDesc* desc, const void* initial_data)
	{
		ensure(desc);

		BufferRef buffer = MakeShared<D3D12Buffer>();
		D3D12Buffer* dxBuffer = static_cast<D3D12Buffer*>(buffer.Get());
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

		HRESULT hr = m_Device->GetAllocator()->CreateResource(&allocationDesc,
									&CD3DX12_RESOURCE_DESC::Buffer(buffer->size),
									ConvertToDXResourceState(state),
									nullptr,
									&dxBuffer->allocation,
									IID_PPV_ARGS(&dxBuffer->resource));

		ensure(hr == S_OK);

		dxBuffer->resource->Map(0, nullptr, &buffer->mappedData);
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

			uint32_t index = m_Device->GetSRVDescriptorHeap()->AllocateHandle();
			dxBuffer->cpuHandle = m_Device->GetSRVDescriptorHeap()->GetCPUHandle(index);
			m_Device->GetDevice()->CreateShaderResourceView(dxBuffer->resource.Get(), &srvDesc, dxBuffer->cpuHandle);
			dxBuffer->gpuHandle = m_Device->GetSRVDescriptorHeap()->GetGPUHandle(index);
		}
		break;
		case BufferDesc::Uniform:
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = dxBuffer->resource->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = buffer->size;

			uint32_t index = m_Device->GetSRVDescriptorHeap()->AllocateHandle();
			dxBuffer->cpuHandle = m_Device->GetSRVDescriptorHeap()->GetCPUHandle(index);
			m_Device->GetDevice()->CreateConstantBufferView(&cbvDesc, dxBuffer->cpuHandle);
			dxBuffer->gpuHandle = m_Device->GetSRVDescriptorHeap()->GetGPUHandle(index);
		}
		break;
		}

		return buffer;
	}

	void D3D12DynamicRHI::CreateAttachments(RenderPassRef renderPass)
	{
		D3D12RenderPass* dxRenderPass = static_cast<D3D12RenderPass*>(renderPass.Get());

		bool bIsResize = dxRenderPass->rtvDescriptorsIndices.size() > 0;

		if (renderPass->desc.bIsSwapchainTarget)
		{
			// Create a RTV for each frame
			for (uint32_t i = 0; i < GFrameCount; ++i)
			{
				TextureRef attachment = MakeShared<D3D12Texture>();
				attachment->imageFormat = Format::kRGBA8_UNORM;
				renderPass->colorAttachments.emplace_back(attachment);

				auto dxAttachment = static_cast<D3D12Texture*>(attachment.Get());

				DX_CHECK(m_Swapchain->GetBuffer(i, IID_PPV_ARGS(&dxAttachment->resource)));

				uint32_t rtvIndex;
				if (!bIsResize)
				{
					rtvIndex = m_Device->GetRTVDescriptorHeap()->AllocateHandle();
					dxRenderPass->rtvDescriptorsIndices.emplace_back(rtvIndex);
				}
				else
				{
					rtvIndex = dxRenderPass->rtvDescriptorsIndices[GFrameCount];
				}
				
				m_Device->GetDevice()->CreateRenderTargetView(dxAttachment->resource.Get(), nullptr, m_Device->GetRTVDescriptorHeap()->GetCPUHandle(rtvIndex));

				std::wstring rtName;
				Utils::StringConvert(renderPass->desc.debugName, rtName);;
				rtName += L"_backbuffer";
				dxAttachment->resource->SetName(rtName.c_str());
				attachment->desc.width = renderPass->desc.width;
				attachment->desc.height = renderPass->desc.height;
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

				auto createAttachmentResources = [&](TextureRef attachment, D3D12Texture* textureState, uint32_t attachmentRTVIndex)
				{
					D3D12_RESOURCE_DESC resourceDesc = {};
					resourceDesc.Format = ConvertToDXFormat(format);
					resourceDesc.Width = renderPass->desc.width;
					resourceDesc.Height = renderPass->desc.height;
					resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
					resourceDesc.MipLevels = 1;
					resourceDesc.DepthOrArraySize = 1;
					resourceDesc.SampleDesc.Count = 1;
					resourceDesc.SampleDesc.Quality = 0;
					resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

					D3D12_CLEAR_VALUE clearValue = { ConvertToDXFormat(format), { 0.f, 0.f, 0.f, 0.f } };

					attachment->currentState = ResourceState::kPixelShader;

					D3D12MA::ALLOCATION_DESC allocationDesc = {};
					allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

					m_Device->GetAllocator()->CreateResource(&allocationDesc, &resourceDesc,
						ConvertToDXResourceState(attachment->currentState),
						&clearValue, &textureState->allocation,
						IID_PPV_ARGS(&textureState->resource));

					uint32_t rtvIndex = 0;
					if (!bIsResize)
					{
						rtvIndex = m_Device->GetRTVDescriptorHeap()->AllocateHandle();
						dxRenderPass->rtvDescriptorsIndices.emplace_back(rtvIndex);
					}
					else
					{
						rtvIndex = attachmentRTVIndex;
					}
					
					m_Device->GetDevice()->CreateRenderTargetView(textureState->resource.Get(), nullptr, m_Device->GetRTVDescriptorHeap()->GetCPUHandle(rtvIndex));

					attachment->desc.width = renderPass->desc.width;
					attachment->desc.height = renderPass->desc.height;
					std::wstring rtName;
					Utils::StringConvert(renderPass->desc.debugName, rtName);
					rtName += L"_colorAttachment";
					textureState->resource->SetName(rtName.c_str());
					textureState->resource->SetName(rtName.c_str());
					m_Device->GetDevice()->CreateShaderResourceView(textureState->resource.Get(), nullptr, textureState->cpuHandle);

					attachmentIndex++;
				};

				// If the amount of color attachments is equal to the amouth of formats, than its a resize operation
				if (renderPass->colorAttachments.size() == attachmentCount)
				{
					TextureRef attachment = renderPass->colorAttachments[attachmentIndex];
					D3D12Texture* tex_internal_state = static_cast<D3D12Texture*>(attachment.Get());
					createAttachmentResources(attachment, tex_internal_state, dxRenderPass->rtvDescriptorsIndices[attachmentIndex]);
				}
				else
				{
					auto index = m_Device->GetSRVDescriptorHeap()->AllocateHandle();
					TextureRef attachment = MakeShared<D3D12Texture>();
					attachment->imageFormat = format;
					D3D12Texture* dxAttachment = static_cast<D3D12Texture*>(attachment.Get());
					dxAttachment->cpuHandle = m_Device->GetSRVDescriptorHeap()->GetCPUHandle(index);
					dxAttachment->gpuHandle = m_Device->GetSRVDescriptorHeap()->GetGPUHandle(index);
					renderPass->colorAttachments.emplace_back(attachment);
					createAttachmentResources(attachment, dxAttachment, -1);
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
				format = ConvertToDXFormat(depthFormat);
				resourceFormat = DXGI_FORMAT_R24G8_TYPELESS;
				srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			}
			else if (depthFormat == Format::kDepth32_FLOAT)
			{
				format = ConvertToDXFormat(depthFormat);
				resourceFormat = DXGI_FORMAT_R32_TYPELESS;
				srvFormat = DXGI_FORMAT_R32_FLOAT;
			}
			else if (depthFormat == Format::kDepth32FloatStencil8X24_UINT)
			{
				format = ConvertToDXFormat(depthFormat);
				resourceFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
				srvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
			}

			if (!renderPass->depthStencil)
				renderPass->depthStencil = MakeShared<D3D12Texture>();
			renderPass->depthStencil->imageFormat = depthFormat;
			renderPass->depthStencil->currentState = ResourceState::kDepthWrite;

			D3D12Texture* dxDSVTexture = static_cast<D3D12Texture*>(renderPass->depthStencil.Get());

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

			m_Device->GetAllocator()->CreateResource(&allocationDesc, &resourceDesc,
										ConvertToDXResourceState(renderPass->depthStencil->currentState),
										&depthOptimizedClearValue, &dxDSVTexture->allocation,
										IID_PPV_ARGS(&dxDSVTexture->resource));

			D3D12_DEPTH_STENCIL_VIEW_DESC depthStencil_desc = {};
			depthStencil_desc.Format = format;
			depthStencil_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			depthStencil_desc.Flags = D3D12_DSV_FLAG_NONE;

			uint32_t dsvIndex;
			if (!bIsResize)
			{
				dsvIndex = m_Device->GetDSVDescriptorHeap()->AllocateHandle();
				dxRenderPass->dsvDescriptorIndex = dsvIndex;
			} 
			else
			{
				dsvIndex = dxRenderPass->dsvDescriptorIndex;
			}
			
			auto dsvHandle = m_Device->GetDSVDescriptorHeap()->GetCPUHandle(dsvIndex);
			m_Device->GetDevice()->CreateDepthStencilView(dxDSVTexture->resource.Get(), &depthStencil_desc, dsvHandle);

			auto srvIndex = m_Device->GetSRVDescriptorHeap()->AllocateHandle();
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = srvFormat;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			dxDSVTexture->cpuHandle = m_Device->GetSRVDescriptorHeap()->GetCPUHandle(srvIndex);
			m_Device->GetDevice()->CreateShaderResourceView(dxDSVTexture->resource.Get(), &srvDesc, dxDSVTexture->cpuHandle);
			dxDSVTexture->gpuHandle = m_Device->GetSRVDescriptorHeap()->GetGPUHandle(srvIndex);

			std::wstring dsvName;
			Utils::StringConvert(renderPass->desc.debugName, dsvName);
			dsvName += L"_depthAttachment";
			dxDSVTexture->resource->SetName(dsvName.c_str());
			dxDSVTexture->allocation->SetName(dsvName.c_str());
		}
	}

	void D3D12DynamicRHI::CreateGraphicsPipeline(PipelineRef pipeline, PipelineDesc* desc)
	{
		// Get shader file path
		std::wstring shaderPath = L"shaders/";
		shaderPath.append(std::wstring(desc->programName.begin(), desc->programName.end()));
		shaderPath.append(L".hlsl");

		D3D12Pipeline* dxPipeline = static_cast<D3D12Pipeline*>(pipeline.Get());
		auto vertexShader = CompileShader(shaderPath, ShaderStage::kVS);
		dxPipeline->vertexReflection = vertexShader.reflection;
		auto pixel_shader = CompileShader(shaderPath, ShaderStage::kPS);
		dxPipeline->pixel_reflection = pixel_shader.reflection;

		ED_LOG_INFO("Compiled program '{}' successfully!", desc->programName);

		// Create the root signature
		CreateRootSignature(pipeline);

		// Get input elements from vertex shader reflection
		std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
		D3D12_SHADER_DESC shaderDesc;
		dxPipeline->vertexReflection->GetDesc(&shaderDesc);
		for (uint32_t i = 0; i < shaderDesc.InputParameters; ++i)
		{
			D3D12_SIGNATURE_PARAMETER_DESC desc = {};
			dxPipeline->vertexReflection->GetInputParameterDesc(i, &desc);

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
		renderTargetBlendDesc.BlendEnable = desc->bEnableBlending; // #todo: blending is not showing the best looking results :)
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
		psoDesc.pRootSignature = dxPipeline->rootSignature.Get();
		psoDesc.VS = { vertexShader.blob->GetBufferPointer(), vertexShader.blob->GetBufferSize() };
		psoDesc.PS = { pixel_shader.blob->GetBufferPointer(), pixel_shader.blob->GetBufferSize() };
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = ConvertToDXCullMode(desc->cull_mode);
		psoDesc.RasterizerState.FrontCounterClockwise = desc->bIsFrontCounterClockwise;
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthFunc = ConvertToDXComparisonFunc(desc->depthFunc);
		psoDesc.DSVFormat = ConvertToDXFormat(desc->renderPass->depthStencil->imageFormat);
		psoDesc.InputLayout.NumElements = static_cast<uint32_t>(inputElements.size());
		psoDesc.InputLayout.pInputElementDescs = inputElements.data();
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.SampleDesc.Count = 1;

		uint32_t attachmentsIndex = 0;
		for (auto& attachment : desc->renderPass->colorAttachments)
		{
			psoDesc.RTVFormats[attachmentsIndex] = ConvertToDXFormat(attachment->imageFormat);
			psoDesc.BlendState.RenderTarget[0] = renderTargetBlendDesc;
			attachmentsIndex++;
		}
		attachmentsIndex++;
		psoDesc.NumRenderTargets = attachmentsIndex;

		DX_CHECK(m_Device->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&dxPipeline->pipelineState)));

		std::wstring pipelineName;
		Utils::StringConvert(desc->programName, pipelineName);
		pipelineName.append(L"_PSO");
		dxPipeline->pipelineState->SetName(pipelineName.c_str());
		Utils::StringConvert(pipelineName, pipeline->desc.debugName);
	}

	void D3D12DynamicRHI::CreateComputePipeline(PipelineRef pipeline, PipelineDesc* desc)
	{
		// Get shader file path
		std::wstring shaderPath = L"shaders/";
		shaderPath.append(std::wstring(desc->programName.begin(), desc->programName.end()));
		shaderPath.append(L".hlsl");

		ShaderResult computeShader = CompileShader(shaderPath, ShaderStage::kCS);
		D3D12Pipeline* dxPipeline = static_cast<D3D12Pipeline*>(pipeline.Get());
		dxPipeline->computeReflection = computeShader.reflection;

		ED_LOG_INFO("Compiled compute program '{}' successfully!", desc->programName);

		// Create the root signature
		CreateRootSignature(pipeline);

		// Describe and create pipeline state object(PSO)
		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = dxPipeline->rootSignature.Get();
		psoDesc.CS = { computeShader.blob->GetBufferPointer(), computeShader.blob->GetBufferSize() };
		DX_CHECK(m_Device->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&dxPipeline->pipelineState)));

		std::wstring pipelineName;
		Utils::StringConvert(desc->programName, pipelineName);
		pipelineName.append(L"_PSO");
		dxPipeline->pipelineState->SetName(pipelineName.c_str());
		Utils::StringConvert(pipelineName, pipeline->desc.debugName);
	}

	void D3D12DynamicRHI::CreateCommands()
	{
		// Describe and create graphics command queue
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		DX_CHECK(m_Device->GetDevice()->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_CommandQueue)))

		// Create graphics command allocator
		for (int i = 0; i < GFrameCount; ++i)
		{
			DX_CHECK(m_Device->GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator[i])));
			m_CommandAllocator[i]->SetName(L"gfxCommandAllocator");
		}

		// Create graphics command list
		DX_CHECK(m_Device->GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator[m_FrameIndex].Get(), nullptr, IID_PPV_ARGS(&m_CommandList)));
		m_CommandList->SetName(L"gfxCommandList");
	}

	void D3D12DynamicRHI::EnsureMsgResourceState(TextureRef resource, ResourceState destResourceState)
	{
		ensure(resource);

		if (resource->currentState != destResourceState)
		{
			ChangeResourceState(resource, resource->currentState, destResourceState);
			resource->currentState = destResourceState;
		}
	}

	PipelineRef D3D12DynamicRHI::CreatePipeline(PipelineDesc* desc)
	{
		ensure(desc);

		PipelineRef pipeline = MakeShared<D3D12Pipeline>();
		pipeline->rootParameterIndices.clear();
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

		return pipeline;
	}

	TextureRef D3D12DynamicRHI::CreateTexture(std::string filePath, bool bGenerateMips)
	{
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
				return nullptr;
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
				return nullptr;
			}

		}
			
		desc.data = textureFile;
		desc.width = width;
		desc.height = height;
		desc.bIsStorage = false;

		return CreateTexture(&desc);
	}

	TextureRef D3D12DynamicRHI::CreateTexture(TextureDesc* desc)
	{
		ensureMsg(desc->type == TextureDesc::Texture2D, "Only texture2d is implemented!");

		TextureRef texture = MakeShared<D3D12Texture>();
		D3D12Texture* dxTexture = static_cast<D3D12Texture*>(texture.Get());
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
		textureDesc.Format = ConvertToDXFormat(texture->imageFormat);
		textureDesc.Width = texture->desc.width;
		textureDesc.Height = texture->desc.height;
		textureDesc.Flags = flags;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		D3D12MA::ALLOCATION_DESC allocationDesc = {};
		allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

		HRESULT hr = m_Device->GetAllocator()->CreateResource(&allocationDesc,
									&textureDesc,
									ConvertToDXResourceState(destResourceState),
									nullptr,
									&dxTexture->allocation,
									IID_PPV_ARGS(&dxTexture->resource));

		ensure(hr == S_OK);

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
		srvDesc.Format = ConvertToDXFormat(texture->imageFormat);
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = texture->mipCount;

		auto srvIndex = m_Device->GetSRVDescriptorHeap()->AllocateHandle();
		dxTexture->cpuHandle = m_Device->GetSRVDescriptorHeap()->GetCPUHandle(srvIndex);
		m_Device->GetDevice()->CreateShaderResourceView(dxTexture->resource.Get(), &srvDesc, dxTexture->cpuHandle);
		dxTexture->gpuHandle = m_Device->GetSRVDescriptorHeap()->GetGPUHandle(srvIndex);

		if (desc->bIsStorage)
		{
			auto uavIndex  = m_Device->GetSRVDescriptorHeap()->AllocateHandle();
			auto cpuHandle = m_Device->GetSRVDescriptorHeap()->GetCPUHandle(uavIndex);
			m_Device->GetDevice()->CreateUnorderedAccessView(dxTexture->resource.Get(), nullptr, nullptr, cpuHandle);
		}

		if (desc->debugName.size() > 0) 
		{
			std::wstring wname;
			Utils::StringConvert(desc->debugName, wname);
			dxTexture->resource->SetName(wname.c_str());
		}

		return texture;
	}

	RenderPassRef D3D12DynamicRHI::CreateRenderPass(RenderPassDesc* desc)
	{
		ensure(desc);
		ensureMsg(desc->width > 0, "Render Pass width has to be > 0");
		ensureMsg(desc->height > 0, "Render Pass height has to be > 0");

		RenderPassRef renderPass = MakeShared<D3D12RenderPass>();
		renderPass->desc = *desc;

		uint32_t amountOfDepthFormats = 0;
		for (Format& format : renderPass->desc.attachmentsFormats)
		{
			if (IsDepthFormat(format))
				amountOfDepthFormats++;
		}

		ensureMsg(amountOfDepthFormats < 2, "Can't create render passes with more than 1 depth target");
		ensureMsg(desc->attachmentsFormats.size() > 0, "Can't create render pass without attachments");
		ensureMsg(desc->attachmentsFormats.size() < 8, "Can't create a render pass with more than 8 attachments");

		// NOTE: For now it is only possible to have one depth attachment
		CreateAttachments(renderPass);

		return renderPass;
	}

	GPUTimerRef D3D12DynamicRHI::CreateGPUTimer()
	{
		GPUTimerRef timer = MakeShared<D3D12GPUTimer>();
		D3D12GPUTimer* dxTimer = static_cast<D3D12GPUTimer*>(timer.Get());

		D3D12_QUERY_HEAP_DESC heapDesc = { };
		heapDesc.Count = 2;
		heapDesc.NodeMask = 0;
		heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		HRESULT hr = m_Device->GetDevice()->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&dxTimer->queryHeap));
		ensure(hr == S_OK);

		BufferDesc readbackDesc		= {};
		readbackDesc.elementCount	= 2;
		readbackDesc.stride			= sizeof(uint64_t);
		readbackDesc.usage			= BufferDesc::Readback;
		timer->readbackBuffer		= CreateBuffer(&readbackDesc, nullptr);

		return timer;
	}

	void D3D12DynamicRHI::BeginGPUTimer(GPUTimerRef timer)
	{
		ensure(timer);

		D3D12GPUTimer* dxTimer = static_cast<D3D12GPUTimer*>(timer.Get());
		m_CommandList->EndQuery(dxTimer->queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
	}

	void D3D12DynamicRHI::EndGPUTimer(GPUTimerRef timer)
	{
		ensure(timer);

		D3D12GPUTimer* dxTimer = static_cast<D3D12GPUTimer*>(timer.Get());
		D3D12Buffer*   dxReadbackBuffer = static_cast<D3D12Buffer*>(timer->readbackBuffer.Get());

		uint64_t gpuFrequency;
		m_CommandQueue->GetTimestampFrequency(&gpuFrequency);

		// Insert the end timestamp
		m_CommandList->EndQuery(dxTimer->queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);

		// Resolve the data
		m_CommandList->ResolveQueryData(dxTimer->queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, dxReadbackBuffer->resource.Get(), 0);

		// Get the query data
		uint64_t* queryData = reinterpret_cast<uint64_t*>(timer->readbackBuffer->mappedData);
		uint64_t startTime = queryData[0];
		uint64_t endTime = queryData[1];

		if (endTime > startTime)
		{
			uint64_t delta = endTime - startTime;
			double frequency = double(gpuFrequency);
			timer->elapsedTime = (delta / frequency) * 1000.0;
		}
	}

	GfxResult D3D12DynamicRHI::UpdateBufferData(BufferRef buffer, const void* data, uint32_t count)
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

	void D3D12DynamicRHI::GenerateMips(TextureRef texture)
	{
		ensure(texture);
		D3D12Texture* dxTexture = static_cast<D3D12Texture*>(texture.Get());

		//Prepare the shader resource view description for the source texture
		D3D12_SHADER_RESOURCE_VIEW_DESC srcTextureSrvDesc = {};
		srcTextureSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srcTextureSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

		//Prepare the unordered access view description for the destination texture
		D3D12_UNORDERED_ACCESS_VIEW_DESC destTextureUavDesc = {};
		destTextureUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		BindPipeline(m_MipsPipeline);
		BindSRVDescriptorHeap();

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
				auto srvIndex = m_Device->GetSRVDescriptorHeap()->AllocateHandle();
				srcTextureSrvDesc.Format = ConvertToDXFormat(texture->imageFormat);
				srcTextureSrvDesc.Texture2D.MipLevels = 1;
				srcTextureSrvDesc.Texture2D.MostDetailedMip = i;
				m_Device->GetDevice()->CreateShaderResourceView(dxTexture->resource.Get(), &srcTextureSrvDesc, m_Device->GetSRVDescriptorHeap()->GetCPUHandle(srvIndex));
				BindRootParameter("SrcTexture", m_Device->GetSRVDescriptorHeap()->GetGPUHandle(srvIndex));
			}

			//Create unordered access view for the destination texture in the descriptor heap
			{
				auto uavIndex = m_Device->GetSRVDescriptorHeap()->AllocateHandle();
				destTextureUavDesc.Format = ConvertToDXFormat(texture->imageFormat);
				destTextureUavDesc.Texture2D.MipSlice = i + 1;
				m_Device->GetDevice()->CreateUnorderedAccessView(dxTexture->resource.Get(), nullptr, &destTextureUavDesc, m_Device->GetSRVDescriptorHeap()->GetCPUHandle(uavIndex));
				BindRootParameter("DstTexture", m_Device->GetSRVDescriptorHeap()->GetGPUHandle(uavIndex));
			}

			//Pass the destination texture pixel size to the shader as constants
			BindParameter("TexelSize", &texel_size, sizeof(glm::vec2));

			m_CommandList->Dispatch(std::max<uint32_t>(width / 8, 1u), std::max<uint32_t>(height / 8, 1u), 1);
			
			//Wait for all accesses to the destination texture UAV to be finished before generating the next mipmap, as it will be the source texture for the next mipmap
			m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(dxTexture->resource.Get()));

			ChangeResourceState(texture, ResourceState::kNonPixelShader, ResourceState::kUnorderedAccess, (int)srcTexture);
		}
	}

	uint64_t D3D12DynamicRHI::GetTextureID(TextureRef texture)
	{
		ensure(texture);

		D3D12Texture* dxTexture = static_cast<D3D12Texture*>(texture.Get());
		return dxTexture->gpuHandle.ptr;
	}

	GfxResult D3D12DynamicRHI::ReloadPipeline(PipelineRef pipeline)
	{
		if (pipeline == nullptr) return GfxResult::kInvalidParameter;

		pipeline = CreatePipeline(&pipeline->desc);

		return GfxResult::kNoError;
	}

	void D3D12DynamicRHI::EnableImGui()
	{
		// Setup imgui
		uint32_t index = m_Device->GetSRVDescriptorHeap()->AllocateHandle();
		ImGui_ImplDX12_Init(m_Device->GetDevice(), GFrameCount, DXGI_FORMAT_R8G8B8A8_UNORM, m_Device->GetSRVDescriptorHeap()->GetHeap(),
							m_Device->GetSRVDescriptorHeap()->GetCPUHandle(index), m_Device->GetSRVDescriptorHeap()->GetGPUHandle(index));
		m_bIsImguiInitialized = true;

		ImGuiNewFrame();
	}

	void D3D12DynamicRHI::ImGuiNewFrame()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void D3D12DynamicRHI::BindPipeline(PipelineRef pipeline)
	{
		ensure(pipeline);

		D3D12Pipeline* dxPipeline = static_cast<D3D12Pipeline*>(pipeline.Get());

		if (pipeline->desc.type == kPipelineType_Graphics)
			m_CommandList->SetGraphicsRootSignature(dxPipeline->rootSignature.Get());
		else if (pipeline->desc.type == kPipelineType_Compute)
			m_CommandList->SetComputeRootSignature(dxPipeline->rootSignature.Get());
		
		m_CommandList->SetPipelineState(dxPipeline->pipelineState.Get());
		m_BoundPipeline = pipeline;
	}

	void D3D12DynamicRHI::BindVertexBuffer(BufferRef vertexBuffer)
	{
		ensure(vertexBuffer);

		D3D12Buffer* dxBuffer = static_cast<D3D12Buffer*>(vertexBuffer.Get());
		ensureMsg(dxBuffer->resource != nullptr, "Can't bind a empty vertex buffer!");
		ensureMsg(m_BoundPipeline->desc.type == kPipelineType_Graphics, "Can't bind a vertex buffer on a compute pipeline!");

		m_BoundVertexBuffer.BufferLocation	= dxBuffer->resource->GetGPUVirtualAddress();
		m_BoundVertexBuffer.SizeInBytes		= vertexBuffer->size;
		m_BoundVertexBuffer.StrideInBytes	= vertexBuffer->desc.stride;
	}

	void D3D12DynamicRHI::BindIndexBuffer(BufferRef indexBuffer)
	{
		ensure(indexBuffer);

		D3D12Buffer* dxBuffer = static_cast<D3D12Buffer*>(indexBuffer.Get());
		ensureMsg(dxBuffer->resource != nullptr, "Can't bind a empty index buffer!");
		ensureMsg(m_BoundPipeline->desc.type == kPipelineType_Graphics, "Can't bind a index buffer on a compute pipeline!");

		m_BoundIndexBuffer.BufferLocation	= dxBuffer->resource->GetGPUVirtualAddress();
		m_BoundIndexBuffer.SizeInBytes		= indexBuffer->size;
		m_BoundIndexBuffer.Format			= DXGI_FORMAT_R32_UINT;
	}

	void D3D12DynamicRHI::BindParameter(const std::string& parameterName, BufferRef buffer)
	{
		ensure(buffer);

		D3D12Buffer* dxBuffer = static_cast<D3D12Buffer*>(buffer.Get());
		BindRootParameter(parameterName, dxBuffer->gpuHandle);
	}

	void D3D12DynamicRHI::BindParameter(const std::string& parameterName, TextureRef texture, TextureUsage usage)
	{
		ensure(texture);

		D3D12Texture* dxTexture = static_cast<D3D12Texture*>(texture.Get());

		auto gpuHandle = dxTexture->gpuHandle;
		if (usage == kReadWrite)
		{
			gpuHandle.ptr += static_cast<uint64_t>(m_Device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

			EnsureMsgResourceState(texture, ResourceState::kUnorderedAccess);
		}
		else
		{
			EnsureMsgResourceState(texture, ResourceState::kPixelShader);
		}
		BindRootParameter(parameterName, gpuHandle);
	}

	void D3D12DynamicRHI::BindParameter(const std::string& parameterName, void* data, size_t size)
	{
		ensure(data);

		uint32_t rootParameterIndex = GetRootParameterIndex(parameterName);
		if (rootParameterIndex == -1)
		{
			//ED_LOG_WARN("Parameter {} was not found, skiping it!", parameterName);
			return;
		}
		uint32_t num_values = static_cast<uint32_t>(size / sizeof(uint32_t));
		if (m_BoundPipeline->desc.type == kPipelineType_Graphics)
			m_CommandList->SetGraphicsRoot32BitConstants(rootParameterIndex, num_values, data, 0);
		else if (m_BoundPipeline->desc.type == kPipelineType_Compute)
			m_CommandList->SetComputeRoot32BitConstants(rootParameterIndex, num_values, data, 0);
	}

	void D3D12DynamicRHI::BindRootParameter(const std::string& parameterName, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
	{
		uint32_t rootParameterIndex = GetRootParameterIndex(parameterName);
		if (rootParameterIndex == -1)
		{
			//ED_LOG_WARN("Parameter {} was not found, skiping it!", parameterName);
			return;
		}
		if (m_BoundPipeline->desc.type == kPipelineType_Graphics)
			m_CommandList->SetGraphicsRootDescriptorTable(rootParameterIndex, gpuHandle);
		else if (m_BoundPipeline->desc.type == kPipelineType_Compute)
			m_CommandList->SetComputeRootDescriptorTable(rootParameterIndex, gpuHandle);
	}

	void D3D12DynamicRHI::SetTextureData(TextureRef texture)
	{
		D3D12Texture* dxTexture = static_cast<D3D12Texture*>(texture.Get());
		uint32_t dataCount = texture->mipCount;

		const uint64_t uploadBufferSize = GetRequiredIntermediateSize(dxTexture->resource.Get(), 0, dataCount);

		// Create the GPU upload buffer
		D3D12MA::ALLOCATION_DESC uploadAllocationDesc = {};
		uploadAllocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

		m_Device->GetAllocator()->CreateResource(&uploadAllocationDesc,
									&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
									D3D12_RESOURCE_STATE_GENERIC_READ,
									nullptr,
									&dxTexture->uploadAllocation,
									IID_PPV_ARGS(&dxTexture->uploadHeap));

		// Copy data to the intermediate upload heap and then schedule a copy 
		// from the upload heap to the Texture2D.
		dxTexture->data.resize(dataCount);
		
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
			dxTexture->data[i] = textureData;

			width = std::max<uint32_t>(width / 2, 1u);
			height = std::max<uint32_t>(height / 2, 1u);
		}
		dxTexture->resource->SetName(L"texture_resource");
		dxTexture->uploadHeap->SetName(L"texture");
		UpdateSubresources(m_CommandList.Get(), dxTexture->resource.Get(), dxTexture->uploadHeap.Get(), 0, 0, (uint32_t)dxTexture->data.size(), dxTexture->data.data());
	}

	void D3D12DynamicRHI::BeginRender()
	{
		// Set SRV Descriptor heap
		BindSRVDescriptorHeap();
	}

	void D3D12DynamicRHI::BeginRenderPass(RenderPassRef renderPass)
	{
		ensure(renderPass);

		D3D12RenderPass* dxRenderPass = static_cast<D3D12RenderPass*>(renderPass.Get());

		PIXBeginEvent(m_CommandList.Get(), PIX_COLOR(0, 0, 0), renderPass->desc.debugName.c_str());

		float* clearColor = (float*)&renderPass->desc.clearColor;
		CD3DX12_CLEAR_VALUE clearValue{ DXGI_FORMAT_R32G32B32_FLOAT, clearColor };

		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE beginAccessType = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
		D3D12_RENDER_PASS_ENDING_ACCESS_TYPE endAccessType = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

		D3D12_RENDER_PASS_BEGINNING_ACCESS rtvBeginAccess{ beginAccessType, { clearValue } };
		D3D12_RENDER_PASS_ENDING_ACCESS rtvEndAccess{ endAccessType, {} };

		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_Device->GetDSVDescriptorHeap()->GetCPUHandle(dxRenderPass->dsvDescriptorIndex);

		std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> renderTargetDescriptions;
		if (renderPass->desc.bIsSwapchainTarget)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_Device->GetRTVDescriptorHeap()->GetCPUHandle(dxRenderPass->rtvDescriptorsIndices[m_FrameIndex]);
			D3D12_RENDER_PASS_RENDER_TARGET_DESC rtvDesc = { rtvHandle, rtvBeginAccess, rtvEndAccess };
			renderTargetDescriptions.emplace_back(rtvDesc);
			
			EnsureMsgResourceState(renderPass->colorAttachments[m_FrameIndex], ResourceState::kRenderTarget);
		}
		else
		{
			int attachmentIndex = 0;
			for (auto& attachment : renderPass->colorAttachments)
			{
				if (renderPass->desc.width != attachment->desc.width ||
					renderPass->desc.height != attachment->desc.height)
				{
					CreateAttachments(renderPass);
				}
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_Device->GetRTVDescriptorHeap()->GetCPUHandle(dxRenderPass->rtvDescriptorsIndices[attachmentIndex]);
				D3D12_RENDER_PASS_RENDER_TARGET_DESC rtvDesc = { rtvHandle, rtvBeginAccess, rtvEndAccess };
				renderTargetDescriptions.emplace_back(rtvDesc);
				EnsureMsgResourceState(attachment, ResourceState::kRenderTarget);
				++attachmentIndex;
			}
		}

		// DSV
		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* dsvDesc = nullptr;
		if (renderPass->depthStencil.IsValid())
		{
			D3D12_RENDER_PASS_BEGINNING_ACCESS_CLEAR_PARAMETERS dsvClear = {};
			dsvClear.ClearValue.Format = ConvertToDXFormat(renderPass->depthStencil->imageFormat);
			dsvClear.ClearValue.DepthStencil.Depth = 1.0f;
			D3D12_RENDER_PASS_BEGINNING_ACCESS dsvBeginAccess{ beginAccessType, { dsvClear } };
			D3D12_RENDER_PASS_ENDING_ACCESS dsvEndAccess{ endAccessType, {} };
			D3D12_RENDER_PASS_DEPTH_STENCIL_DESC desc = { dsvHandle, dsvBeginAccess, dsvBeginAccess, dsvEndAccess, dsvEndAccess };
			dsvDesc = &desc;
		}

		m_CommandList->BeginRenderPass(static_cast<uint32_t>(renderTargetDescriptions.size()),
		                               renderTargetDescriptions.data(),
		                               dsvDesc, D3D12_RENDER_PASS_FLAG_NONE);
		
	}

	void D3D12DynamicRHI::SetSwapchainTarget(RenderPassRef renderPass)
	{
		ensure(renderPass);

		ensureMsg(renderPass->desc.bIsSwapchainTarget, "SetSwapchainTarget render pass was not created as a swapchain target!");
		m_SwapchainTarget = renderPass;
	}

	void D3D12DynamicRHI::EndRenderPass(RenderPassRef renderPass)
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
			EnsureMsgResourceState(renderPass->colorAttachments[m_FrameIndex], ResourceState::kPresent);
		}
		else
		{
			for (auto& attachment : renderPass->colorAttachments)
				EnsureMsgResourceState(attachment, ResourceState::kPixelShader);
		}
	}

	void D3D12DynamicRHI::EndRender()
	{
		if (m_bIsImguiInitialized)
			ImGuiNewFrame();
	}

	void D3D12DynamicRHI::PrepareDraw()
	{
		D3D12Pipeline* dxPipeline = static_cast<D3D12Pipeline*>(m_BoundPipeline.Get());

		ensureMsg(dxPipeline->pipelineState != nullptr, "Can't Draw without a valid pipeline bound!");
		ensureMsg(dxPipeline->rootSignature != nullptr, "Can't Draw without a valid pipeline bound!");

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

	void D3D12DynamicRHI::Draw(uint32_t vertexCount, uint32_t instanceCount /*= 1*/, uint32_t startVertexLocation /*= 0*/, uint32_t startInstanceLocation /*= 0*/)
	{
		PrepareDraw();

		m_CommandList->IASetVertexBuffers(0, 1, &m_BoundVertexBuffer);
		m_CommandList->DrawInstanced(vertexCount, instanceCount, startVertexLocation, startInstanceLocation);
	}

	void D3D12DynamicRHI::DrawIndexed(uint32_t indexCount, uint32_t instanceCount /*= 1*/, uint32_t startIndexLocation /*= 0*/, uint32_t baseVertexLocation /*= 0*/, uint32_t startInstanceLocation /*= 0*/)
	{
		PrepareDraw();

		m_CommandList->IASetVertexBuffers(0, 1, &m_BoundVertexBuffer);
		m_CommandList->IASetIndexBuffer(&m_BoundIndexBuffer);
		m_CommandList->DrawIndexedInstanced(indexCount, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
	}

	void D3D12DynamicRHI::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
	{
		m_CommandList->Dispatch(groupCountX, groupCountY, groupCountZ);
	}

	void D3D12DynamicRHI::Render()
	{
		DX_CHECK(m_CommandList->Close());

		ID3D12CommandList* commandLists[] = { m_CommandList.Get() };
		m_CommandQueue->ExecuteCommandLists(ARRAYSIZE(commandLists), commandLists);

		uint64_t currentFenceValue = m_FenceValues[m_FrameIndex];
		WaitForGPU();

		if (!m_VSyncEnabled)
			m_Swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
		else
			m_Swapchain->Present(1, 0);

		m_CommandAllocator[m_FrameIndex]->Reset();
		m_CommandList->Reset(m_CommandAllocator[m_FrameIndex].Get(), nullptr);

		m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();
		m_FenceValues[m_FrameIndex] = currentFenceValue + 1;
	}

	void D3D12DynamicRHI::WaitForGPU()
	{
		// Schedule a signal command in the queue
		m_CommandQueue->Signal(m_Fence.Get(), m_FenceValues[m_FrameIndex]);

		// Wait until the fence has been processed
		m_Fence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_FenceEvent);
		WaitForSingleObject(m_FenceEvent, INFINITE);

		// Increment the fence value for the current frame
		m_FenceValues[m_FrameIndex]++;
	}

	GfxResult D3D12DynamicRHI::Resize(uint32_t width, uint32_t height)
	{
		ensureMsg(m_SwapchainTarget, "Failed to resize, no Render Target Render Pass assigned");

		if (m_Device->GetDevice() && m_Swapchain)
		{
			ED_LOG_TRACE("Resizing Window to {}x{}", width, height);

			WaitForGPU();
			if (FAILED(m_CommandList->Close()))
			{
				ensureMsg(false, "Failed to close command list");
				return GfxResult::kInternalError;
			}

			for (size_t i = 0; i < GFrameCount; ++i)
			{
				D3D12Texture* dxRenderTarget = static_cast<D3D12Texture*>(m_SwapchainTarget->colorAttachments[i].Get());
				dxRenderTarget->resource.Reset();
				m_FenceValues[i] = m_FenceValues[m_FrameIndex];
			}

			m_SwapchainTarget->depthStencil = {};

			DXGI_SWAP_CHAIN_FLAG flags = {};
			if (!m_VSyncEnabled)
				flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
			DX_CHECK(m_Swapchain->ResizeBuffers(GFrameCount, width, height, DXGI_FORMAT_UNKNOWN, flags));

			m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

			m_SwapchainTarget->colorAttachments.clear();
			m_SwapchainTarget->desc.width = width;
			m_SwapchainTarget->desc.height = height;
			CreateAttachments(m_SwapchainTarget);
			
			m_Scissor = CD3DX12_RECT(0, 0, width, height);

			m_CommandAllocator[m_FrameIndex]->Reset();
			m_CommandList->Reset(m_CommandAllocator[m_FrameIndex].Get(), nullptr);
		}

		return GfxResult::kNoError;
	}

	void D3D12DynamicRHI::ReadPixelFromTexture(uint32_t x, uint32_t y, TextureRef texture, glm::vec4& pixel)
	{
		ensure(texture);

		D3D12Texture* dxTexture = static_cast<D3D12Texture*>(texture.Get());

		uint64_t width = texture->desc.width * 32;
		width = ALIGN(width, 256);

		uint64_t stagingBufferSize = width * texture->desc.height;

		D3D12MA::ALLOCATION_DESC allocationDesc = {};
		allocationDesc.HeapType = D3D12_HEAP_TYPE_READBACK;

		HRESULT hr = m_Device->GetAllocator()->CreateResource(&allocationDesc,
									&CD3DX12_RESOURCE_DESC::Buffer(stagingBufferSize),
									D3D12_RESOURCE_STATE_COPY_DEST,
									nullptr,
									&pixelReadStagingBuffer.allocation,
									IID_PPV_ARGS(&pixelReadStagingBuffer.resource));
		ensure(hr == S_OK);

		D3D12_SUBRESOURCE_FOOTPRINT subresourceFootprint = {};
		subresourceFootprint.Width = texture->desc.width;
		subresourceFootprint.Height = texture->desc.height;
		subresourceFootprint.Depth = 1;
		subresourceFootprint.Format = ConvertToDXFormat(texture->imageFormat);
		subresourceFootprint.RowPitch = static_cast<uint32_t>(width);

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT textureFootprint = {};
		textureFootprint.Footprint = subresourceFootprint;
		textureFootprint.Offset = 0;

		D3D12_TEXTURE_COPY_LOCATION src = {};
		src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src.SubresourceIndex = 0;
		src.pResource = dxTexture->resource.Get();
		 
		D3D12_TEXTURE_COPY_LOCATION dst = {};
		dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dst.PlacedFootprint = textureFootprint;
		dst.pResource = pixelReadStagingBuffer.resource.Get();

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
		m_CommandList->Reset(m_CommandAllocator[m_FrameIndex].Get(), nullptr);
		WaitForGPU();
		BindSRVDescriptorHeap();

		void* pixel_data;
		pixelReadStagingBuffer.resource->Map(0, nullptr, &pixel_data);
		pixel = *(glm::vec4*)pixel_data;
	}

	void D3D12DynamicRHI::ChangeResourceState(TextureRef resource, ResourceState currentState, ResourceState desiredState, int subresource /*= -1*/)
	{
		ensure(resource);

		D3D12Texture* dxTexture = static_cast<D3D12Texture*>(resource.Get());
		uint32_t sub = subresource == -1 ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : (uint32_t)subresource;

		m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
												dxTexture->resource.Get(), 
												ConvertToDXResourceState(currentState),
												ConvertToDXResourceState(desiredState),
												sub));
	}

	void D3D12DynamicRHI::BindSRVDescriptorHeap()
	{
		ID3D12DescriptorHeap* heaps[] = { m_Device->GetSRVDescriptorHeap()->GetHeap() };
		m_CommandList->SetDescriptorHeaps(_countof(heaps), heaps);
	}

	uint32_t D3D12DynamicRHI::GetRootParameterIndex(const std::string& parameterName)
	{
		auto rootParameterIndex = m_BoundPipeline->rootParameterIndices.find(parameterName.data());

		bool bWasRootParameterFound = rootParameterIndex != m_BoundPipeline->rootParameterIndices.end();
		if (!bWasRootParameterFound)
			return -1;
			
		ensureMsg(bWasRootParameterFound, "Failed to find root parameter!");

		return rootParameterIndex->second;
	}
}