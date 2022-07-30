#include "D3D12RHI.h"

#include <cstdint>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <imgui/backends/imgui_impl_dx12.h>
#include <imgui/backends/imgui_impl_win32.h>

#include <dxgidebug.h>
#include <dxgi1_6.h>

#include "Core/Memory.h"
#include "Utilities/Utils.h"
#include "Profiling/Profiler.h"
#include "UI/ImGuizmo.h"

// From Guillaume Boiss� "gfx" https://github.com/gboisse/gfx/blob/b83878e562c2c205000b19c99cf24b13973dedb2/gfx_core.h#L77
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

	D3D12RenderPass* ToInternal(const RenderPass* render_pass)
	{
		return static_cast<D3D12RenderPass*>(render_pass->internal_state.get());
	}

	D3D12GPUTimer* ToInternal(const GPUTimer* gpu_timer)
	{
		return static_cast<D3D12GPUTimer*>(gpu_timer->internal_state.get());
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

	void D3D12RHI::Init(Window* window)
	{
		m_Scissor = CD3DX12_RECT(0, 0, window->GetWidth(), window->GetHeight());
		m_CurrentAPI = D3D12;

		uint32_t dxgi_factory_flags = 0;

	#ifdef ED_DEBUG
		// NOTE: Enabling the debug layer after device creation will invalidate the active device.
		{
			ComPtr<ID3D12Debug> debug_controller;
			if (!FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
			{
				debug_controller->EnableDebugLayer();

				ComPtr<ID3D12Debug1> debug_controller1;
				if (FAILED(debug_controller->QueryInterface(IID_PPV_ARGS(&debug_controller1))))
					ED_LOG_WARN("Failed to get ID3D12Debug1!");

				debug_controller1->SetEnableGPUBasedValidation(true);
			}

			ComPtr<IDXGIInfoQueue> dxgi_info_queue;
			if (!FAILED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_info_queue))))
			{
				dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;

				dxgi_info_queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
				dxgi_info_queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
			}
		}
	#endif

		if (FAILED(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&m_Factory))))
			ED_ASSERT_MB(false, "Failed to create dxgi factory!");

		GetHardwareAdapter();

		if (FAILED(D3D12CreateDevice(m_Adapter.Get(), s_FeatureLevel, IID_PPV_ARGS(&m_Device))))
			ED_ASSERT_MB(false, "Faile to create device");

	#ifdef ED_DEBUG
		{
			if (!FAILED(m_Device->QueryInterface(IID_PPV_ARGS(&m_InfoQueue))))
			{
				ComPtr<ID3D12InfoQueue1> info_queue1;
				
				if (!FAILED(m_InfoQueue->QueryInterface(IID_PPV_ARGS(&info_queue1))))
					info_queue1->RegisterMessageCallback(DebugMessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &m_MessageCallbackCookie);
			}
		}
	#endif

		m_Device->SetName(L"gfx_device");

		DXGI_ADAPTER_DESC1 adapter_desc;
		m_Adapter->GetDesc1(&adapter_desc);
		std::string name;
		Utils::StringConvert(adapter_desc.Description, name);
		ED_LOG_INFO("Initialized D3D12 device on {}", name);

		// Create allocator
		D3D12MA::ALLOCATOR_DESC allocator_desc = {};
		allocator_desc.pDevice = m_Device.Get();
		allocator_desc.pAdapter = m_Adapter.Get();

		if (FAILED(D3D12MA::CreateAllocator(&allocator_desc, &m_Allocator)))
			ED_ASSERT_MB(false, "Failed to create allocator!");

		// Create commands
		CreateCommands();

		// Describe and create the swap chain
		DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
		swapchain_desc.Width = window->GetWidth();
		swapchain_desc.Height = window->GetHeight();
		swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchain_desc.SampleDesc.Count = 1;
		swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchain_desc.BufferCount = s_FrameCount;
		swapchain_desc.Scaling = DXGI_SCALING_NONE;
		swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

		ComPtr<IDXGISwapChain1> swapchain;
		if (FAILED(m_Factory->CreateSwapChainForHwnd(m_CommandQueue.Get(), window->GetHandle(), &swapchain_desc, nullptr, nullptr, &swapchain)))
			ED_ASSERT_MB(false, "Failed to create swapchain!");

		// NOTE(Daniel): Disable fullscreen
		m_Factory->MakeWindowAssociation(window->GetHandle(), DXGI_MWA_NO_ALT_ENTER);

		swapchain.As(&m_Swapchain);
		m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

		// Describe and create a shader resource view (SRV) heap for the texture.
		m_SRVHeap = CreateDescriptorHeap(s_SRVDescriptorCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

		// Create synchronization objects
		{
			if (FAILED(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence))))
				ED_ASSERT_MB(false, "Failed to create fence!");
			m_FenceValues[m_FrameIndex]++;

			// Create an event handler to use for frame synchronization
			m_FenceEvent = CreateEvent(nullptr, false, false, nullptr);
			ED_ASSERT_MB(m_FenceEvent != nullptr, HRESULT_FROM_WIN32(GetLastError()));

			// Wait for the command list to execute; we are reusing the same command 
			// list in our main loop but for now, we just want to wait for setup to 
			// complete before continuing.
			WaitForGPU();
		}

		// Associate the graphics device Resize with the window resize callback
		window->SetResizeCallback([&](uint32_t x, uint32_t y) { Resize(x, y); });

		// Setup profiler
		ID3D12Device* pDevice = m_Device.Get();
		ID3D12CommandQueue* pCommandQueue = m_CommandQueue.Get();
		ED_PROFILE_GPU_INIT(pDevice, &pCommandQueue, 1);

		// Create mips pipeline
		PipelineDesc mips_desc = {};
		mips_desc.program_name = "GenerateMips";
		mips_desc.type = Compute;
		m_MipsPipeline = CreatePipeline(&mips_desc);
	}

	void D3D12RHI::Shutdown()
	{
		WaitForGPU();

		if (m_ImguiInitialized)
		{
			ImGui_ImplDX12_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
		}

		CloseHandle(m_FenceEvent);

		ED_PROFILER_SHUTDOWN();
	}

	D3D12RHI::ShaderResult D3D12RHI::CompileShader(std::filesystem::path file_path, ShaderStage stage)
	{
		ED_PROFILE_FUNCTION();

		std::wstring entry_point = L"";
		std::wstring stage_str = L"";

		switch (stage)
		{
		case VS:
			entry_point = L"VSMain";
			stage_str = L"vs_6_0";
			break;
		case PS:
			entry_point = L"PSMain";
			stage_str = L"ps_6_0";
			break;
		case CS:
			entry_point = L"CSMain";
			stage_str = L"cs_6_0";
			break;
		}

		std::wstring wpdb_name = file_path.stem().wstring() + L"_" + stage_str + L".pdb";

		std::vector arguments =
		{
			file_path.c_str(),
			L"-E", entry_point.c_str(),
			L"-T", stage_str.c_str(),
			L"-Zi",
			L"-Fd", wpdb_name.c_str()
		};

		ComPtr<IDxcBlobEncoding> source = nullptr;
		m_DxcUtils->LoadFile(file_path.c_str(), nullptr, &source);
		if (!source)
		{
			ED_LOG_ERROR("Could not find the program shader file: {}", file_path);
			ED_ASSERT(false);
		}

		DxcBuffer source_buffer;
		source_buffer.Ptr = source->GetBufferPointer();
		source_buffer.Size = source->GetBufferSize();
		source_buffer.Encoding = DXC_CP_ACP;

		ComPtr<IDxcResult> result;
		m_DxcCompiler->Compile(&source_buffer, arguments.data(), (uint32_t)arguments.size(), m_DxcIncludeHandler.Get(), IID_PPV_ARGS(&result));

		// Print errors if present.
		ComPtr<IDxcBlobUtf8> errors = nullptr;
		result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
		// Note that d3dcompiler would return null if no errors or warnings are present.  
		// IDxcCompiler3::Compile will always return an error buffer, but its length will be zero if there are no warnings or errors.
		if (errors != nullptr && errors->GetStringLength() != 0)
			ED_LOG_ERROR("Failed to compile {} {} shader: {}", file_path.filename(), Utils::ShaderStageToString(stage), errors->GetStringPointer());

		// Save pdb
		ComPtr<IDxcBlob> pdb = nullptr;
		ComPtr<IDxcBlobUtf16> pdb_name = nullptr;
		result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pdb), &pdb_name);
		{
			if (!std::filesystem::exists("shaders/intermediate/"))
				std::filesystem::create_directory("shaders/intermediate");

			std::wstring pdb_path = L"shaders/intermediate/";
			pdb_path.append(pdb_name->GetStringPointer());

			FILE* fp = NULL;
			_wfopen_s(&fp, pdb_path.c_str(), L"wb");
			fwrite(pdb->GetBufferPointer(), pdb->GetBufferSize(), 1, fp);
			fclose(fp);
		}

		// Get shader blob
		ComPtr<IDxcBlob> shader_blob;
		result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_blob), nullptr);

		// Get reflection blob
		ComPtr<IDxcBlob> reflection_blob;
		result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflection_blob), nullptr);

		DxcBuffer reflection_buffer;
		reflection_buffer.Ptr = reflection_blob->GetBufferPointer();
		reflection_buffer.Size = reflection_blob->GetBufferSize();
		reflection_buffer.Encoding = DXC_CP_ACP;

		ComPtr<ID3D12ShaderReflection> reflection;
		m_DxcUtils->CreateReflection(&reflection_buffer, IID_PPV_ARGS(&reflection));

		return { shader_blob, reflection };
	}

	D3D12_STATIC_SAMPLER_DESC D3D12RHI::CreateSamplerDesc(uint32_t shader_register, uint32_t register_space, D3D12_SHADER_VISIBILITY shader_visibility, D3D12_TEXTURE_ADDRESS_MODE address_mode)
	{
		D3D12_STATIC_SAMPLER_DESC sampler;
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = address_mode;
		sampler.AddressV = sampler.AddressU;
		sampler.AddressW = sampler.AddressU;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MipLODBias = 0;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = shader_register;
		sampler.RegisterSpace = register_space;
		sampler.ShaderVisibility = shader_visibility;

		return sampler;
	}

	void D3D12RHI::CreateRootSignature(std::shared_ptr<Pipeline>& pipeline)
	{
		auto internal_state = ToInternal(pipeline.get());

		D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};

		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(m_Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
		{
			ED_LOG_WARN("Root signature version 1.1 not available, switching to 1.0");
			feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		uint32_t srv_count = 0;
		std::vector<D3D12_ROOT_PARAMETER1> root_parameters;
		std::vector<D3D12_DESCRIPTOR_RANGE1> descriptor_ranges;
		std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;
		
		std::vector<uint32_t> shader_counts;
		if (internal_state->vertex_reflection)
			shader_counts.emplace_back(ShaderStage::VS);
		if (internal_state->pixel_reflection)
			shader_counts.emplace_back(ShaderStage::PS);
		if (internal_state->compute_reflection)
			shader_counts.emplace_back(ShaderStage::CS);
		uint32_t shader_count = static_cast<uint32_t>(shader_counts.size());

		for (uint32_t i = shader_counts[0]; i < shader_counts[0] + shader_count; ++i)
		{
			ComPtr<ID3D12ShaderReflection> reflection;
			D3D12_SHADER_VISIBILITY shader_visibility;

			// Get shader stage
			switch (i)
			{
			case ShaderStage::VS:
				reflection = internal_state->vertex_reflection;
				shader_visibility = D3D12_SHADER_VISIBILITY_VERTEX;
				break;
			case ShaderStage::PS:
				reflection = internal_state->pixel_reflection;
				shader_visibility = D3D12_SHADER_VISIBILITY_PIXEL;
				break;
			case ShaderStage::CS:
				reflection = internal_state->compute_reflection;
				shader_visibility = D3D12_SHADER_VISIBILITY_ALL;
				break;
			default:
				ED_LOG_ERROR("Shader stage is not yet implemented!");
				break;
			}

			if (!reflection) return;
			D3D12_SHADER_DESC shader_desc = {};
			reflection->GetDesc(&shader_desc);

			for (uint32_t j = 0; j < shader_desc.BoundResources; ++j)
			{
				D3D12_SHADER_INPUT_BIND_DESC desc = {};
				reflection->GetResourceBindingDesc(j, &desc);

				// NOTE(Daniel): Add more as needed
				switch (desc.Type)
				{
				case D3D_SIT_STRUCTURED:
				case D3D_SIT_TEXTURE:
					{
						auto root_parameter_index = pipeline->root_parameter_indices.find(desc.Name);
						bool root_parameter_found = root_parameter_index != pipeline->root_parameter_indices.end();
						if (root_parameter_found) continue;

						D3D12_DESCRIPTOR_RANGE1 descriptor_range = {};
						descriptor_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
						descriptor_range.NumDescriptors = desc.BindCount;
						descriptor_range.BaseShaderRegister = desc.BindPoint;
						descriptor_range.RegisterSpace = desc.Space;
						descriptor_range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE; // check if this is needed
						descriptor_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
						descriptor_ranges.emplace_back(descriptor_range);

						D3D12_ROOT_PARAMETER1 root_parameter = {};
						root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
						root_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

						pipeline->root_parameter_indices[desc.Name] = static_cast<uint32_t>(root_parameters.size()); // get the root parameter index
						root_parameters.emplace_back(root_parameter);
					}
					break;
				case D3D_SIT_CBUFFER:
					{
						D3D12_ROOT_PARAMETER1 root_parameter = {};

						ID3D12ShaderReflectionConstantBuffer* constant_buffer = reflection->GetConstantBufferByName(desc.Name);
						D3D12_SHADER_BUFFER_DESC buffer_desc = {};
						constant_buffer->GetDesc(&buffer_desc);

						if (buffer_desc.Variables == 1)
						{
							ID3D12ShaderReflectionVariable* root_variable = constant_buffer->GetVariableByIndex(0);
							D3D12_SHADER_VARIABLE_DESC variable_desc = {};
							root_variable->GetDesc(&variable_desc);

							D3D12_ROOT_CONSTANTS constants;
							constants.ShaderRegister = desc.BindPoint;
							constants.RegisterSpace = desc.Space;
							constants.Num32BitValues = variable_desc.Size / sizeof(uint32_t);

							root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
							root_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
							root_parameter.Constants = constants;

							descriptor_ranges.emplace_back(); // Emtpy descriptor range
						}
						else if (buffer_desc.Variables > 1)
						{
							auto root_parameter_index = pipeline->root_parameter_indices.find(desc.Name);
							bool root_parameter_found = root_parameter_index != pipeline->root_parameter_indices.end();
							if (root_parameter_found) continue;

							D3D12_DESCRIPTOR_RANGE1 descriptor_range = {};
							descriptor_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
							descriptor_range.NumDescriptors = desc.BindCount;
							descriptor_range.BaseShaderRegister = desc.BindPoint;
							descriptor_range.RegisterSpace = desc.Space;
							descriptor_range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
							descriptor_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
							descriptor_ranges.emplace_back(descriptor_range);

							root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
							root_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
						}

						pipeline->root_parameter_indices[desc.Name] = static_cast<uint32_t>(root_parameters.size()); // get the root parameter index
						root_parameters.emplace_back(root_parameter);
					}
					break;
				case D3D_SIT_UAV_RWTYPED:
					{
						auto root_parameter_index = pipeline->root_parameter_indices.find(desc.Name);
						bool root_parameter_found = root_parameter_index != pipeline->root_parameter_indices.end();
						if (root_parameter_found) continue;

						D3D12_DESCRIPTOR_RANGE1 descriptor_range = {};
						descriptor_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
						descriptor_range.NumDescriptors = desc.BindCount;
						descriptor_range.BaseShaderRegister = desc.BindPoint;
						descriptor_range.RegisterSpace = desc.Space;
						descriptor_range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE; // check if this is needed
						descriptor_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
						descriptor_ranges.emplace_back(descriptor_range);

						D3D12_ROOT_PARAMETER1 root_parameter = {};
						root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
						root_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
						pipeline->root_parameter_indices[desc.Name] = static_cast<uint32_t>(root_parameters.size()); // get the root parameter index
						root_parameters.emplace_back(root_parameter);
					}
					break;
				case D3D_SIT_SAMPLER:
					{
						// Right now is only creating linear samplers
						//! This gets the samplers on Global.hlsli, don't create samplers on a specific shader
						D3D12_STATIC_SAMPLER_DESC sampler;
						std::string sampler_name = std::string(desc.Name);
						if (sampler_name == "LinearClamp")
							sampler = CreateSamplerDesc(desc.BindPoint, desc.Space, shader_visibility, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
						else if (sampler_name == "LinearWrap")
							sampler = CreateSamplerDesc(desc.BindPoint, desc.Space, shader_visibility, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
						samplers.emplace_back(sampler);
					}
					break;
				default:
					ED_LOG_ERROR("Resource type has not been yet implemented!");
					break;
				}
			}
		}

		for (size_t i = 0; i < root_parameters.size(); ++i)
		{
			auto& parameter = root_parameters[i];
			if (parameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) continue;
			parameter.DescriptorTable.pDescriptorRanges = &descriptor_ranges[i];
			parameter.DescriptorTable.NumDescriptorRanges = 1;
		}

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc = {};
		uint32_t num_root_parameters = static_cast<uint32_t>(root_parameters.size());
		uint32_t num_samplers = static_cast<uint32_t>(samplers.size());
		root_signature_desc.Init_1_1(num_root_parameters, root_parameters.data(), num_samplers, samplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;

		if (FAILED(D3DX12SerializeVersionedRootSignature(&root_signature_desc, feature_data.HighestVersion, &signature, &error)))
			ED_LOG_ERROR("Failed to serialize root signature: {}", (char*)error->GetBufferPointer());

		if (FAILED(m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&internal_state->root_signature))))
			ED_LOG_ERROR("Failed to create root signature");
	}

	std::shared_ptr<Buffer> D3D12RHI::CreateBuffer(BufferDesc* desc, const void* initial_data)
	{
		std::shared_ptr<Buffer> buffer = std::make_shared<Buffer>();
		auto internal_state = std::make_shared<D3D12Buffer>();
		buffer->internal_state = internal_state;
		buffer->desc = *desc;

		buffer->size = buffer->desc.element_count * buffer->desc.stride;
		if (buffer->desc.usage == BufferDesc::Uniform)
			buffer->size = ALIGN(buffer->size, 256);

		auto heap_type = D3D12_HEAP_TYPE_UPLOAD;
		auto state = ResourceState::READ;
		if (buffer->desc.usage == BufferDesc::Readback)
		{
			heap_type = D3D12_HEAP_TYPE_READBACK;
			state = ResourceState::COPY_DEST;
			ED_ASSERT_LOG(!initial_data, "A readback buffer cannot have initial data!");
			initial_data = nullptr;
		}

		D3D12MA::ALLOCATION_DESC allocation_desc = {};
		allocation_desc.HeapType = heap_type;

		m_Allocator->CreateResource(&allocation_desc,
									&CD3DX12_RESOURCE_DESC::Buffer(buffer->size),
									Helpers::ConvertResourceState(state),
									nullptr,
									&internal_state->allocation,
									IID_PPV_ARGS(&internal_state->resource));

		internal_state->resource->Map(0, nullptr, &buffer->mapped_data);
		if (initial_data)
		{
			memcpy(buffer->mapped_data, initial_data, buffer->size);
			buffer->initialized = true;
		}

		switch (buffer->desc.usage)
		{
		case BufferDesc::Storage:
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.Format = DXGI_FORMAT_UNKNOWN;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srv_desc.Buffer.FirstElement = 0;
			srv_desc.Buffer.NumElements = buffer->desc.element_count;
			srv_desc.Buffer.StructureByteStride = buffer->desc.stride;
			srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

			auto offset = AllocateHandle(m_SRVHeap);
			internal_state->cpu_handle = GetCPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, offset);
			m_Device->CreateShaderResourceView(internal_state->resource.Get(), &srv_desc, internal_state->cpu_handle);
			internal_state->gpu_handle = GetGPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, offset);
		}
		break;
		case BufferDesc::Uniform:
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
			cbv_desc.BufferLocation = internal_state->resource->GetGPUVirtualAddress();
			cbv_desc.SizeInBytes = buffer->size;

			auto offset = AllocateHandle(m_SRVHeap);
			internal_state->cpu_handle = GetCPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, offset);
			m_Device->CreateConstantBufferView(&cbv_desc, internal_state->cpu_handle);
			internal_state->gpu_handle = GetGPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, offset);
		}
		break;
		}

		return buffer;
	}

	void D3D12RHI::CreateAttachments(std::shared_ptr<RenderPass>& render_pass)
	{
		auto internal_state = ToInternal(render_pass.get());

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(GetCPUHandle(internal_state->rtv_heap));
		if (render_pass->desc.swapchain_target)
		{
			// Create a RTV for each frame
			for (uint32_t i = 0; i < s_FrameCount; ++i)
			{
				auto attachment = std::make_shared<Texture>();
				attachment->internal_state = std::make_shared<D3D12Texture>();
				attachment->image_format = Format::RGBA8_UNORM;
				render_pass->color_attachments.emplace_back(attachment);

				auto attachment_internal_state = ToInternal(attachment.get());

				if (FAILED(m_Swapchain->GetBuffer(i, IID_PPV_ARGS(&attachment_internal_state->resource))))
					ED_ASSERT_MB(false, "Failed to get render target from swapchain!");

				m_Device->CreateRenderTargetView(attachment_internal_state->resource.Get(), nullptr, rtv_handle);
				rtv_handle.Offset(1, m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

				std::wstring rt_name;
				Utils::StringConvert(render_pass->desc.debug_name, rt_name);;
				rt_name += L"backbuffer";
				attachment_internal_state->resource->SetName(rt_name.c_str());
				attachment->desc.width = render_pass->desc.width;
				attachment->desc.height = render_pass->desc.height;
			}
		}
		else
		{
			uint32_t attachment_index = 0;
			for (auto& format : render_pass->desc.attachments_formats)
			{
				if (IsDepthFormat(format))
					continue;

				size_t attachment_count = render_pass->desc.attachments_formats.size();
				if (GetDepthFormatIndex(render_pass->desc.attachments_formats) > -1)
					attachment_count = render_pass->desc.attachments_formats.size() - 1;

				std::shared_ptr<Texture> attachment;
				D3D12Texture* attachment_internal_state;
				// If the amount of color attachments is equal to the amouth of formats, than its a resize operation
				if (render_pass->color_attachments.size() == attachment_count)
				{
					attachment = render_pass->color_attachments[attachment_index];
					attachment_internal_state = ToInternal(attachment.get());
				}
				else
				{
					auto offset = AllocateHandle(m_SRVHeap);
					attachment = std::make_shared<Texture>();
					attachment->internal_state = std::make_shared<D3D12Texture>();
					attachment->image_format = format;
					attachment_internal_state = ToInternal(attachment.get());
					attachment_internal_state->cpu_handle = GetCPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, offset);
					attachment_internal_state->gpu_handle = GetGPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, offset);
					render_pass->color_attachments.emplace_back(attachment);
				}

				D3D12_RESOURCE_DESC resource_desc = {};
				resource_desc.Format = Helpers::ConvertFormat(format);
				resource_desc.Width = render_pass->desc.width;
				resource_desc.Height = render_pass->desc.height;
				resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				resource_desc.MipLevels = 1;
				resource_desc.DepthOrArraySize = 1;
				resource_desc.SampleDesc.Count = 1;
				resource_desc.SampleDesc.Quality = 0;
				resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
				
				D3D12_CLEAR_VALUE clear_value = { Helpers::ConvertFormat(format), { 0.f, 0.f, 0.f, 0.f } };

				attachment->current_state = ResourceState::PIXEL_SHADER;

				D3D12MA::ALLOCATION_DESC allocation_desc = {};
				allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

				m_Allocator->CreateResource(&allocation_desc, &resource_desc, 
											Helpers::ConvertResourceState(attachment->current_state), 
											&clear_value, &attachment_internal_state->allocation, 
											IID_PPV_ARGS(&attachment_internal_state->resource));

				m_Device->CreateRenderTargetView(attachment_internal_state->resource.Get(), nullptr, rtv_handle);

				attachment->desc.width = render_pass->desc.width;
				attachment->desc.height = render_pass->desc.height;
				std::wstring rt_name;
				Utils::StringConvert(render_pass->desc.debug_name, rt_name);
				rt_name += L"color_attachment";
				attachment_internal_state->resource->SetName(rt_name.c_str());
				attachment_internal_state->resource->SetName(rt_name.c_str());
				m_Device->CreateShaderResourceView(attachment_internal_state->resource.Get(), nullptr, attachment_internal_state->cpu_handle);

				attachment_index++;
			}
		}

		// Create DSV
		auto depth_index = GetDepthFormatIndex(render_pass->desc.attachments_formats);
		if (depth_index > -1)
		{
			Format depth_format = render_pass->desc.attachments_formats[depth_index];
			DXGI_FORMAT format, resource_format, srv_format;
			if (depth_format == Format::DEPTH24_STENCIL8)
			{
				format = Helpers::ConvertFormat(depth_format);
				resource_format = DXGI_FORMAT_R24G8_TYPELESS;
				srv_format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			}
			else if (depth_format == Format::DEPTH32_FLOAT)
			{
				format = Helpers::ConvertFormat(depth_format);
				resource_format = DXGI_FORMAT_R32_TYPELESS;
				srv_format = DXGI_FORMAT_R32_FLOAT;
			}
			else if (depth_format == Format::DEPTH32_FLOAT_STENCIL8_UINT)
			{
				format = Helpers::ConvertFormat(depth_format);
				resource_format = DXGI_FORMAT_R32G8X24_TYPELESS;
				srv_format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
			}

			render_pass->depth_stencil = std::make_shared<Texture>();
			auto dsv_internal_state = std::make_shared<D3D12Texture>();
			render_pass->depth_stencil->internal_state = dsv_internal_state;
			render_pass->depth_stencil->image_format = depth_format;
			render_pass->depth_stencil->current_state = ResourceState::DEPTH_WRITE;

			D3D12_CLEAR_VALUE depth_optimized_clear_value;
			depth_optimized_clear_value.Format = format;
			depth_optimized_clear_value.DepthStencil.Depth = 1.0f;
			depth_optimized_clear_value.DepthStencil.Stencil = 0;

			D3D12_RESOURCE_DESC resource_desc = {};
			resource_desc.Format = resource_format;
			resource_desc.Width = render_pass->desc.width;
			resource_desc.Height = render_pass->desc.height;
			resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resource_desc.MipLevels = 1;
			resource_desc.DepthOrArraySize = 1;
			resource_desc.SampleDesc.Count = 1;
			resource_desc.SampleDesc.Quality = 0;
			resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			D3D12MA::ALLOCATION_DESC allocation_desc = {};
			allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

			m_Allocator->CreateResource(&allocation_desc, &resource_desc,
										Helpers::ConvertResourceState(render_pass->depth_stencil->current_state),
										&depth_optimized_clear_value, &dsv_internal_state->allocation,
										IID_PPV_ARGS(&dsv_internal_state->resource));

			D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_desc = {};
			depth_stencil_desc.Format = format;
			depth_stencil_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			depth_stencil_desc.Flags = D3D12_DSV_FLAG_NONE;

			auto dsv_handle = GetCPUHandle(internal_state->dsv_heap);
			m_Device->CreateDepthStencilView(dsv_internal_state->resource.Get(), &depth_stencil_desc, dsv_handle);

			auto srv_offset = AllocateHandle(m_SRVHeap);
			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.Format = srv_format;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = 1;
			dsv_internal_state->cpu_handle = GetCPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srv_offset);
			m_Device->CreateShaderResourceView(dsv_internal_state->resource.Get(), &srv_desc, dsv_internal_state->cpu_handle);
			dsv_internal_state->gpu_handle = GetGPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srv_offset);

			std::wstring dsv_name;
			Utils::StringConvert(render_pass->desc.debug_name, dsv_name);
			dsv_name += L"depth_attachment";
			dsv_internal_state->resource->SetName(dsv_name.c_str());
			dsv_internal_state->allocation->SetName(dsv_name.c_str());
		}
	}

	void D3D12RHI::CreateGraphicsPipeline(std::shared_ptr<Pipeline>& pipeline, std::shared_ptr<D3D12Pipeline>& internal_state, PipelineDesc* desc)
	{
		// Get shader file path
		std::wstring shader_path = L"shaders/";
		shader_path.append(std::wstring(desc->program_name.begin(), desc->program_name.end()));
		shader_path.append(L".hlsl");

		auto vertex_shader = CompileShader(shader_path, ShaderStage::VS);
		internal_state->vertex_reflection = vertex_shader.reflection;
		auto pixel_shader = CompileShader(shader_path, ShaderStage::PS);
		internal_state->pixel_reflection = pixel_shader.reflection;

		ED_LOG_INFO("Compiled program '{}' successfully!", desc->program_name);

		// Create the root signature
		CreateRootSignature(pipeline);

		// Get input elements from vertex shader reflection
		std::vector<D3D12_INPUT_ELEMENT_DESC> input_elements;
		D3D12_SHADER_DESC shader_desc;
		internal_state->vertex_reflection->GetDesc(&shader_desc);
		for (uint32_t i = 0; i < shader_desc.InputParameters; ++i)
		{
			D3D12_SIGNATURE_PARAMETER_DESC desc = {};
			internal_state->vertex_reflection->GetInputParameterDesc(i, &desc);

			uint32_t component_count = 0;
			while (desc.Mask)
			{
				++component_count;
				desc.Mask >>= 1;
			}

			DXGI_FORMAT component_format = DXGI_FORMAT_UNKNOWN;
			switch (desc.ComponentType)
			{
			case D3D_REGISTER_COMPONENT_UINT32:
				component_format = (component_count == 1 ? DXGI_FORMAT_R32_UINT :
									component_count == 2 ? DXGI_FORMAT_R32G32_UINT :
									component_count == 3 ? DXGI_FORMAT_R32G32B32_UINT :
									DXGI_FORMAT_R32G32B32A32_UINT);
				break;
			case D3D_REGISTER_COMPONENT_SINT32:
				component_format = (component_count == 1 ? DXGI_FORMAT_R32_SINT :
									component_count == 2 ? DXGI_FORMAT_R32G32_SINT :
									component_count == 3 ? DXGI_FORMAT_R32G32B32_SINT :
									DXGI_FORMAT_R32G32B32A32_SINT);
				break;
			case D3D_REGISTER_COMPONENT_FLOAT32:
				component_format = (component_count == 1 ? DXGI_FORMAT_R32_FLOAT :
									component_count == 2 ? DXGI_FORMAT_R32G32_FLOAT :
									component_count == 3 ? DXGI_FORMAT_R32G32B32_FLOAT :
									DXGI_FORMAT_R32G32B32A32_FLOAT);
				break;
			default:
				break;  // D3D_REGISTER_COMPONENT_UNKNOWN
			}

			D3D12_INPUT_ELEMENT_DESC input_element = {};
			input_element.SemanticName = desc.SemanticName;
			input_element.SemanticIndex = desc.SemanticIndex;
			input_element.Format = component_format;
			input_element.InputSlot = 0;
			input_element.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
			input_element.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
			input_element.InstanceDataStepRate = 0;

			input_elements.emplace_back(input_element);
		}

		D3D12_RENDER_TARGET_BLEND_DESC render_target_blend_desc = {};
		render_target_blend_desc.BlendEnable = desc->enable_blending;
		render_target_blend_desc.LogicOpEnable = false;
		render_target_blend_desc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		render_target_blend_desc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		render_target_blend_desc.BlendOp = D3D12_BLEND_OP_ADD;
		render_target_blend_desc.SrcBlendAlpha = D3D12_BLEND_ONE;
		render_target_blend_desc.DestBlendAlpha = D3D12_BLEND_ONE;
		render_target_blend_desc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		render_target_blend_desc.LogicOp = D3D12_LOGIC_OP_CLEAR;
		render_target_blend_desc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		// Describe and create pipeline state object(PSO)
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
		pso_desc.pRootSignature = internal_state->root_signature.Get();
		pso_desc.VS = { vertex_shader.blob->GetBufferPointer(), vertex_shader.blob->GetBufferSize() };
		pso_desc.PS = { pixel_shader.blob->GetBufferPointer(), pixel_shader.blob->GetBufferSize() };
		pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		pso_desc.SampleMask = UINT_MAX;
		pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pso_desc.RasterizerState.CullMode = Helpers::ConvertCullMode(desc->cull_mode);
		pso_desc.RasterizerState.FrontCounterClockwise = desc->front_counter_clockwise;
		pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pso_desc.DepthStencilState.DepthFunc = Helpers::ConvertComparisonFunc(desc->depth_func);
		pso_desc.DSVFormat = Helpers::ConvertFormat(desc->render_pass->depth_stencil->image_format);
		pso_desc.InputLayout.NumElements = static_cast<uint32_t>(input_elements.size());
		pso_desc.InputLayout.pInputElementDescs = input_elements.data();
		pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pso_desc.SampleDesc.Count = 1;

		uint32_t attachments_index = 0;
		for (auto& attachment : desc->render_pass->color_attachments)
		{
			pso_desc.RTVFormats[attachments_index] = Helpers::ConvertFormat(attachment->image_format);
			pso_desc.BlendState.RenderTarget[0] = render_target_blend_desc;
			attachments_index++;
		}
		attachments_index++;
		pso_desc.NumRenderTargets = attachments_index;

		if (FAILED(m_Device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&internal_state->pipeline_state))))
			ED_ASSERT_MB(false, "Failed to create graphics pipeline state!");

		std::wstring pipeline_name;
		Utils::StringConvert(desc->program_name, pipeline_name);
		pipeline_name.append(L"_PSO");
		internal_state->pipeline_state->SetName(pipeline_name.c_str());
		Utils::StringConvert(pipeline_name, pipeline->debug_name);
	}

	void D3D12RHI::CreateComputePipeline(std::shared_ptr<Pipeline>& pipeline, std::shared_ptr<D3D12Pipeline>& internal_state, PipelineDesc* desc)
	{
		// Get shader file path
		std::wstring shader_path = L"shaders/";
		shader_path.append(std::wstring(desc->program_name.begin(), desc->program_name.end()));
		shader_path.append(L".hlsl");

		auto compute_shader = CompileShader(shader_path, ShaderStage::CS);
		internal_state->compute_reflection = compute_shader.reflection;

		ED_LOG_INFO("Compiled compute program '{}' successfully!", desc->program_name);

		// Create the root signature
		CreateRootSignature(pipeline);

		// Describe and create pipeline state object(PSO)
		D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
		pso_desc.pRootSignature = internal_state->root_signature.Get();
		pso_desc.CS = { compute_shader.blob->GetBufferPointer(), compute_shader.blob->GetBufferSize() };
		if (FAILED(m_Device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&internal_state->pipeline_state))))
			ED_ASSERT_MB(false, "Failed to create compute pipeline state!");

		std::wstring pipeline_name;
		Utils::StringConvert(desc->program_name, pipeline_name);
		pipeline_name.append(L"_PSO");
		internal_state->pipeline_state->SetName(pipeline_name.c_str());
		Utils::StringConvert(pipeline_name, pipeline->debug_name);
	}

	void D3D12RHI::CreateCommands()
	{
		// Describe and create graphics command queue
		D3D12_COMMAND_QUEUE_DESC command_queue_desc = {};
		command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		if (FAILED(m_Device->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&m_CommandQueue))))
			ED_ASSERT_MB(false, "Failed to create command queue!");

		// Create graphics command allocator
		if (FAILED(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator))))
			ED_ASSERT_MB(false, "Failed to create command allocator!");
		m_CommandAllocator->SetName(L"gfx_command_allocator");

		// Create graphics command list
		if (FAILED(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_CommandList))))
			ED_ASSERT_MB(false, "Failed to create command list!");
		m_CommandList->SetName(L"gfx_command_list");
	}

	void D3D12RHI::EnsureResourceState(std::shared_ptr<Texture>& resource, ResourceState dest_resource_state)
	{
		if (resource->current_state != dest_resource_state)
		{
			ChangeResourceState(resource, resource->current_state, dest_resource_state);
			resource->current_state = dest_resource_state;
		}
	}

	std::shared_ptr<Pipeline> D3D12RHI::CreatePipeline(PipelineDesc* desc)
	{
		ED_PROFILE_FUNCTION();

		std::shared_ptr<Pipeline> pipeline = std::make_shared<Pipeline>();
		auto internal_state = std::make_shared<D3D12Pipeline>();
		pipeline->internal_state = internal_state;
		pipeline->desc = *desc;

		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_DxcUtils));
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_DxcCompiler));

		// Create default include handler
		m_DxcUtils->CreateDefaultIncludeHandler(&m_DxcIncludeHandler);

		if (desc->type == Graphics)
		{
			ED_ASSERT(desc->render_pass);
			CreateGraphicsPipeline(pipeline, internal_state, desc);
		}
		else if (desc->type == Compute)
		{
			CreateComputePipeline(pipeline, internal_state, desc);
		}

		return pipeline;
	}

	std::shared_ptr<Texture> D3D12RHI::CreateTexture(std::string file_path, bool generate_mips)
	{
		ED_PROFILE_FUNCTION();

		TextureDesc desc = {};
		desc.generate_mips = generate_mips;

		int width, height, nchannels;
		void* texture_file;
		if (stbi_is_hdr(file_path.c_str()))
		{
			// Load texture from file
			texture_file = stbi_loadf(file_path.c_str(), &width, &height, &nchannels, 4);
			if (!texture_file)
			{
				ED_LOG_ERROR("Could not load texture file: {}", file_path);
				return nullptr;
			}

			desc.srgb = true;
		}
		else
		{
			// Load texture from file
			texture_file = stbi_load(file_path.c_str(), &width, &height, &nchannels, 4);
			if (!texture_file)
			{
				ED_LOG_ERROR("Could not load texture file: {}", file_path);
				return nullptr;
			}

		}
			
		desc.data = texture_file;
		desc.width = width;
		desc.height = height;
		desc.storage = false;

		return CreateTexture(&desc);;
	}

	std::shared_ptr<Texture> D3D12RHI::CreateTexture(TextureDesc* desc)
	{
		ED_PROFILE_FUNCTION();

		ED_ASSERT_LOG(desc->type == TextureDesc::Texture2D, "Only texture2d is implemented!");

		std::shared_ptr<Texture> texture = std::make_shared<Texture>();
		auto internal_state = std::make_shared<D3D12Texture>();
		texture->internal_state = internal_state;
		texture->desc = *desc;
		texture->image_format = texture->desc.srgb ? Format::RGBA32_FLOAT : Format::RGBA8_UNORM;
		texture->mip_count = desc->generate_mips ? CalculateMipCount(desc->width, desc->height) : 1;

		auto flags = D3D12_RESOURCE_FLAG_NONE;
		if (desc->storage || desc->generate_mips)
			flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		auto dest_resource_state = ResourceState::PIXEL_SHADER;
		if (desc->storage)
			dest_resource_state = ResourceState::UNORDERED_ACCESS;
		texture->current_state = dest_resource_state;

		// Describe and create a texture2D
		D3D12_RESOURCE_DESC texture_desc = {};
		texture_desc.MipLevels = texture->mip_count;
		texture_desc.Format = Helpers::ConvertFormat(texture->image_format);
		texture_desc.Width = texture->desc.width;
		texture_desc.Height = texture->desc.height;
		texture_desc.Flags = flags;
		texture_desc.DepthOrArraySize = 1;
		texture_desc.SampleDesc.Count = 1;
		texture_desc.SampleDesc.Quality = 0;
		texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		D3D12MA::ALLOCATION_DESC allocation_desc = {};
		allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

		m_Allocator->CreateResource(&allocation_desc,
									&texture_desc,
									Helpers::ConvertResourceState(dest_resource_state),
									nullptr,
									&internal_state->allocation,
									IID_PPV_ARGS(&internal_state->resource));

		if (desc->data)
		{
			EnsureResourceState(texture, ResourceState::COPY_DEST);
			SetTextureData(texture);
			if (desc->generate_mips)
				GenerateMips(texture);
			EnsureResourceState(texture, dest_resource_state);
		}

		// Describe and create a the main SRV for the texture
		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Format = Helpers::ConvertFormat(texture->image_format);
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = texture->mip_count;

		auto srv_offset = AllocateHandle(m_SRVHeap);
		internal_state->cpu_handle = GetCPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srv_offset);
		m_Device->CreateShaderResourceView(internal_state->resource.Get(), &srv_desc, internal_state->cpu_handle);
		internal_state->gpu_handle = GetGPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srv_offset);

		if (desc->storage)
		{
			auto uav_offset = AllocateHandle(m_SRVHeap);
			auto cpu_handle = GetCPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, uav_offset);
			m_Device->CreateUnorderedAccessView(internal_state->resource.Get(), nullptr, nullptr, cpu_handle);
		}

		return texture;
	}

	std::shared_ptr<RenderPass> D3D12RHI::CreateRenderPass(RenderPassDesc* desc)
	{
		ED_ASSERT_LOG(desc->width > 0, "Render Pass width has to be > 0");
		ED_ASSERT_LOG(desc->height > 0, "Render Pass height has to be > 0");

		std::shared_ptr<RenderPass> render_pass = std::make_shared<RenderPass>();
		auto internal_state = std::make_shared<D3D12RenderPass>();
		render_pass->internal_state = internal_state;
		render_pass->desc = *desc;

		internal_state->rtv_heap = CreateDescriptorHeap(s_FrameCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
		internal_state->dsv_heap = CreateDescriptorHeap(1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

		ED_ASSERT_LOG(desc->attachments_formats.size() > 0, "Can't create render pass without attachments");
		ED_ASSERT_LOG(desc->attachments_formats.size() < 8, "Can't create a render pass with more than 8 attachments");

		// NOTE: For now it is only possible to have one depth attachment
		CreateAttachments(render_pass);

		return render_pass;
	}

	std::shared_ptr<GPUTimer> D3D12RHI::CreateGPUTimer()
	{
		std::shared_ptr<GPUTimer> timer = std::make_shared<GPUTimer>();
		auto internal_state = std::make_shared<D3D12GPUTimer>();
		timer->internal_state = internal_state;

		D3D12_QUERY_HEAP_DESC heapDesc = { };
		heapDesc.Count = 2;
		heapDesc.NodeMask = 0;
		heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		m_Device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&internal_state->query_heap));

		BufferDesc readback_desc = {};
		readback_desc.element_count = 2;
		readback_desc.stride = sizeof(uint64_t);
		readback_desc.usage = BufferDesc::Readback;
		timer->readback_buffer = CreateBuffer(&readback_desc, nullptr);

		return timer;
	}

	void D3D12RHI::SetName(std::shared_ptr<Resource> child, std::string& name)
	{
		child->debug_name = name;
		std::wstring wname;
		Utils::StringConvert(name, wname);
		auto internal_state = ToInternal(child.get());
		internal_state->resource->SetName(wname.c_str());
	}

	void D3D12RHI::BeginGPUTimer(std::shared_ptr<GPUTimer>& timer)
	{
		auto internal_state = ToInternal(timer.get());
		m_CommandList->EndQuery(internal_state->query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
	}

	void D3D12RHI::EndGPUTimer(std::shared_ptr<GPUTimer>& timer)
	{
		auto internal_state = ToInternal(timer.get());
		auto readback_internal_state = ToInternal(timer->readback_buffer.get());

		uint64_t gpu_frequency;
		m_CommandQueue->GetTimestampFrequency(&gpu_frequency);

		// Insert the end timestamp
		m_CommandList->EndQuery(internal_state->query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);

		// Resolve the data
		m_CommandList->ResolveQueryData(internal_state->query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, readback_internal_state->resource.Get(), 0);

		// Get the query data
		uint64_t* query_data = reinterpret_cast<uint64_t*>(timer->readback_buffer->mapped_data);
		uint64_t start_time = query_data[0];
		uint64_t end_time = query_data[1];

		if (end_time > start_time)
		{
			uint64_t delta = end_time - start_time;
			double frequency = double(gpu_frequency);
			timer->elapsed_time = (delta / frequency) * 1000.0;
		}
	}

	void D3D12RHI::UpdateBufferData(std::shared_ptr<Buffer>& buffer, const void* data, uint32_t count)
	{
		if (!data) return;
		if (count > 0)
		{
			buffer->desc.element_count = count;
			buffer->size = buffer->desc.element_count * buffer->desc.stride;
		}
		memcpy(buffer->mapped_data, data, buffer->size);
	}

	void D3D12RHI::GenerateMips(std::shared_ptr<Texture>& texture)
	{
		auto internal_state = ToInternal(texture.get());

		//Prepare the shader resource view description for the source texture
		D3D12_SHADER_RESOURCE_VIEW_DESC src_texture_srv_desc = {};
		src_texture_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		src_texture_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

		//Prepare the unordered access view description for the destination texture
		D3D12_UNORDERED_ACCESS_VIEW_DESC dest_texture_uav_desc = {};
		dest_texture_uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		BindPipeline(m_MipsPipeline);
		SetDescriptorHeap(m_SRVHeap);
		EnsureResourceState(texture, ResourceState::UNORDERED_ACCESS);
		for (uint32_t i = 0; i < texture->mip_count - 1; ++i)
		{
			// Update source texture subresource state
			uint32_t src_texture = D3D12CalcSubresource(i, 0, 0, texture->mip_count, 1);
			ChangeResourceState(texture, ResourceState::UNORDERED_ACCESS, ResourceState::NON_PIXEL_SHADER, (int)src_texture);

			//Get mipmap dimensions
			uint32_t width = std::max<uint32_t>(texture->desc.width >> (i + 1), 1);
			uint32_t height = std::max<uint32_t>(texture->desc.height >> (i + 1), 1);
			glm::vec2 texel_size = { 1.0f / width, 1.0f / height };

			//Create shader resource view for the source texture in the descriptor heap
			{
				auto srv_offset = AllocateHandle(m_SRVHeap);
				src_texture_srv_desc.Format = Helpers::ConvertFormat(texture->image_format);
				src_texture_srv_desc.Texture2D.MipLevels = 1;
				src_texture_srv_desc.Texture2D.MostDetailedMip = i;
				m_Device->CreateShaderResourceView(internal_state->resource.Get(), &src_texture_srv_desc, GetCPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srv_offset));
				BindRootParameter("SrcTexture", GetGPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, srv_offset));
			}

			//Create unordered access view for the destination texture in the descriptor heap
			{
				auto uav_offset = AllocateHandle(m_SRVHeap);
				dest_texture_uav_desc.Format = Helpers::ConvertFormat(texture->image_format);
				dest_texture_uav_desc.Texture2D.MipSlice = i + 1;
				m_Device->CreateUnorderedAccessView(internal_state->resource.Get(), nullptr, &dest_texture_uav_desc, GetCPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, uav_offset));
				BindRootParameter("DstTexture", GetGPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, uav_offset));
			}

			//Pass the destination texture pixel size to the shader as constants
			BindParameter("TexelSize", &texel_size, sizeof(glm::vec2));

			m_CommandList->Dispatch(std::max<uint32_t>(width / 8, 1u), std::max<uint32_t>(height / 8, 1u), 1);
			
			//Wait for all accesses to the destination texture UAV to be finished before generating the next mipmap, as it will be the source texture for the next mipmap
			m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(internal_state->resource.Get()));

			ChangeResourceState(texture, ResourceState::NON_PIXEL_SHADER, ResourceState::UNORDERED_ACCESS, (int)src_texture);
		}
	}

	uint64_t D3D12RHI::GetTextureID(std::shared_ptr<Texture>& texture)
	{
		auto internal_state = ToInternal(texture.get());
		return internal_state->gpu_handle.ptr;
	}

	void D3D12RHI::ReloadPipeline(std::shared_ptr<Pipeline>& pipeline)
	{
		pipeline = CreatePipeline(&pipeline->desc);
	}

	void D3D12RHI::EnableImGui()
	{
		// Setup imgui
		auto offset = AllocateHandle(m_SRVHeap);
		ImGui_ImplDX12_Init(m_Device.Get(), s_FrameCount, DXGI_FORMAT_R8G8B8A8_UNORM, m_SRVHeap->heap.Get(),
							GetCPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, offset), GetGPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, offset));
		m_ImguiInitialized = true;

		ImGuiNewFrame();
	}

	void D3D12RHI::ImGuiNewFrame()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void D3D12RHI::BindPipeline(const std::shared_ptr<Pipeline>& pipeline)
	{
		auto internal_state = ToInternal(pipeline.get());

		if (pipeline->desc.type == Graphics)
			m_CommandList->SetGraphicsRootSignature(internal_state->root_signature.Get());
		else if (pipeline->desc.type == Compute)
			m_CommandList->SetComputeRootSignature(internal_state->root_signature.Get());
		
		m_CommandList->SetPipelineState(internal_state->pipeline_state.Get());
		m_BoundPipeline = pipeline;
	}

	void D3D12RHI::BindVertexBuffer(std::shared_ptr<Buffer>& vertex_buffer)
	{
		auto internal_state = ToInternal(vertex_buffer.get());
		ED_ASSERT_LOG(internal_state->resource != nullptr, "Can't bind a empty vertex buffer!");
		ED_ASSERT_LOG(m_BoundPipeline->desc.type == Graphics, "Can't bind a vertex buffer on a compute pipeline!");

		m_BoundVertexBuffer.BufferLocation	= internal_state->resource->GetGPUVirtualAddress();
		m_BoundVertexBuffer.SizeInBytes		= vertex_buffer->size;
		m_BoundVertexBuffer.StrideInBytes	= vertex_buffer->desc.stride;
	}

	void D3D12RHI::BindIndexBuffer(std::shared_ptr<Buffer>& index_buffer)
	{
		auto internal_state = ToInternal(index_buffer.get());
		ED_ASSERT_LOG(internal_state->resource != nullptr, "Can't bind a empty index buffer!");
		ED_ASSERT_LOG(m_BoundPipeline->desc.type == Graphics, "Can't bind a index buffer on a compute pipeline!");

		m_BoundIndexBuffer.BufferLocation	= internal_state->resource->GetGPUVirtualAddress();
		m_BoundIndexBuffer.SizeInBytes		= index_buffer->size;
		m_BoundIndexBuffer.Format			= DXGI_FORMAT_R32_UINT;
	}

	void D3D12RHI::BindParameter(const std::string& parameter_name, std::shared_ptr<Buffer>& buffer)
	{
		auto internal_state = ToInternal(buffer.get());
		BindRootParameter(parameter_name, internal_state->gpu_handle);
	}

	void D3D12RHI::BindParameter(const std::string& parameter_name, std::shared_ptr<Texture>& texture, TextureUsage usage)
	{
		auto internal_state = ToInternal(texture.get());

		auto gpu_handle = internal_state->gpu_handle;
		if (usage == kReadWrite)
		{
			gpu_handle.ptr += static_cast<uint64_t>(m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

			EnsureResourceState(texture, ResourceState::UNORDERED_ACCESS);
		}
		else
		{
			EnsureResourceState(texture, ResourceState::PIXEL_SHADER);
		}
		BindRootParameter(parameter_name, gpu_handle);
	}

	void D3D12RHI::BindParameter(const std::string& parameter_name, void* data, size_t size)
	{
		uint32_t root_parameter_index = GetRootParameterIndex(parameter_name);
		uint32_t num_values = static_cast<uint32_t>(size / sizeof(uint32_t));
		if (m_BoundPipeline->desc.type == Graphics)
			m_CommandList->SetGraphicsRoot32BitConstants(root_parameter_index, num_values, data, 0);
		else if (m_BoundPipeline->desc.type == Compute)
			m_CommandList->SetComputeRoot32BitConstants(root_parameter_index, num_values, data, 0);
	}

	void D3D12RHI::BindRootParameter(const std::string& parameter_name, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
	{
		uint32_t root_parameter_index = GetRootParameterIndex(parameter_name);
		if (m_BoundPipeline->desc.type == Graphics)
			m_CommandList->SetGraphicsRootDescriptorTable(root_parameter_index, gpu_handle);
		else if (m_BoundPipeline->desc.type == Compute)
			m_CommandList->SetComputeRootDescriptorTable(root_parameter_index, gpu_handle);
	}

	void D3D12RHI::SetTextureData(std::shared_ptr<Texture>& texture)
	{
		auto internal_state = ToInternal(texture.get());
		uint32_t data_count = texture->mip_count;

		ComPtr<ID3D12Resource> texture_upload_heap;
		D3D12MA::Allocation* upload_allocation;
		const uint64_t upload_buffer_size = GetRequiredIntermediateSize(internal_state->resource.Get(), 0, data_count);

		// Create the GPU upload buffer
		D3D12MA::ALLOCATION_DESC upload_allocation_desc = {};
		upload_allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

		m_Allocator->CreateResource(&upload_allocation_desc,
									&CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size),
									D3D12_RESOURCE_STATE_GENERIC_READ,
									nullptr,
									&upload_allocation,
									IID_PPV_ARGS(&texture_upload_heap));

		// Copy data to the intermediate upload heap and then schedule a copy 
		// from the upload heap to the Texture2D.
		std::vector<D3D12_SUBRESOURCE_DATA> data(data_count);
		
		uint32_t width = texture->desc.width;
		uint32_t height = texture->desc.height;
		for (uint32_t i = 0; i < data_count; ++i)
		{
			D3D12_SUBRESOURCE_DATA texture_data = {};
			texture_data.pData = texture->desc.data;
			texture_data.RowPitch = width * 4;
			if (texture->desc.srgb)
				texture_data.RowPitch *= sizeof(float);
			texture_data.SlicePitch = height * texture_data.RowPitch;
			data[i] = texture_data;

			width = std::max<uint32_t>(width / 2, 1u);
			height = std::max<uint32_t>(height / 2, 1u);
		}

		UpdateSubresources(m_CommandList.Get(), internal_state->resource.Get(), texture_upload_heap.Get(), 0, 0, data_count, data.data());

		m_CommandList->Close();
		ID3D12CommandList* command_lists[] = { m_CommandList.Get() };
		m_CommandQueue->ExecuteCommandLists(_countof(command_lists), command_lists);
		m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);

		WaitForGPU();

		upload_allocation->Release();
	}

	void D3D12RHI::BeginRender()
	{
		// Set SRV Descriptor heap
		SetDescriptorHeap(m_SRVHeap);
	}

	void D3D12RHI::BeginRenderPass(std::shared_ptr<RenderPass>& render_pass)
	{
		auto internal_state = ToInternal(render_pass.get());

		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = {};
		D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = GetCPUHandle(internal_state->dsv_heap, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 0);
		if (render_pass->desc.swapchain_target)
		{
			rtv_handle = GetCPUHandle(internal_state->rtv_heap, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_FrameIndex);
			EnsureResourceState(render_pass->color_attachments[m_FrameIndex], ResourceState::RENDER_TARGET);
		}
		else
		{
			for (auto& attachment : render_pass->color_attachments)
			{
				if (render_pass->desc.width != attachment->desc.width ||
					render_pass->desc.height != attachment->desc.height)
				{
					CreateAttachments(render_pass);
				}
				rtv_handle = GetCPUHandle(internal_state->rtv_heap, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 0);
				EnsureResourceState(attachment, ResourceState::RENDER_TARGET);
			}
		}

		constexpr float clear_color[] = { 0.15f, 0.15f, 0.15f, 1.0f };
		CD3DX12_CLEAR_VALUE clear_value{ DXGI_FORMAT_R32G32B32_FLOAT, clear_color };

		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE begin_access_type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
		D3D12_RENDER_PASS_ENDING_ACCESS_TYPE end_access_type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

		// RTV
		D3D12_RENDER_PASS_BEGINNING_ACCESS rtv_begin_access{ begin_access_type, { clear_value } };
		D3D12_RENDER_PASS_ENDING_ACCESS rtv_end_access{ end_access_type, {} };
		internal_state->rtv_desc = { rtv_handle, rtv_begin_access, rtv_end_access };

		// DSV
		D3D12_RENDER_PASS_BEGINNING_ACCESS_CLEAR_PARAMETERS dsv_clear = {};
		dsv_clear.ClearValue.Format = Helpers::ConvertFormat(render_pass->depth_stencil->image_format);
		dsv_clear.ClearValue.DepthStencil.Depth = 1.0f;
		D3D12_RENDER_PASS_BEGINNING_ACCESS dsv_begin_access{ begin_access_type, { dsv_clear } };
		D3D12_RENDER_PASS_ENDING_ACCESS dsv_end_access{ end_access_type, {} };
		internal_state->dsv_desc = { dsv_handle, dsv_begin_access, dsv_begin_access, dsv_end_access, dsv_end_access };

		m_CommandList->BeginRenderPass(1, &internal_state->rtv_desc, &internal_state->dsv_desc, D3D12_RENDER_PASS_FLAG_NONE);
	}

	void D3D12RHI::SetSwapchainTarget(std::shared_ptr<RenderPass>& render_pass)
	{
		ED_ASSERT_LOG(render_pass->desc.swapchain_target, "SetSwapchainTarget render pass was not created as a swapchain target!");
		m_SwapchainTarget = render_pass;
	}

	void D3D12RHI::EndRenderPass(std::shared_ptr<RenderPass>& render_pass)
	{
		if (m_ImguiInitialized && render_pass->desc.imgui_pass)
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

		if (render_pass->desc.swapchain_target)
		{
			EnsureResourceState(render_pass->color_attachments[m_FrameIndex], ResourceState::PRESENT);
		}
		else
		{
			for (auto& attachment : render_pass->color_attachments)
				EnsureResourceState(attachment, ResourceState::PIXEL_SHADER);
		}
	}

	void D3D12RHI::EndRender()
	{
		if (m_ImguiInitialized)
			ImGuiNewFrame();
	}

	void D3D12RHI::PrepareDraw()
	{
		auto internal_state = ToInternal(m_BoundPipeline.get());

		ED_ASSERT_LOG(internal_state->pipeline_state != nullptr, "Can't Draw without a valid pipeline bound!");
		ED_ASSERT_LOG(internal_state->root_signature != nullptr, "Can't Draw without a valid pipeline bound!");

		ED_PROFILE_FUNCTION();

		ED_PROFILE_GPU_CONTEXT(m_CommandList.Get());
		ED_PROFILE_GPU_FUNCTION("D3D12RHI::PrepareDraw");

		D3D12_VIEWPORT viewport;
		viewport.Width = static_cast<float>(m_BoundPipeline->desc.render_pass->desc.width);
		viewport.Height = static_cast<float>(m_BoundPipeline->desc.render_pass->desc.height);
		viewport.MaxDepth = 1.0f;
		viewport.MinDepth = m_BoundPipeline->desc.min_depth;
		viewport.TopLeftX = 0.0;
		viewport.TopLeftY = 0.0;
		m_CommandList->RSSetViewports(1, &viewport);
		m_CommandList->RSSetScissorRects(1, &m_Scissor);

		m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	void D3D12RHI::Draw(uint32_t vertex_count, uint32_t instance_count /*= 1*/, uint32_t start_vertex_location /*= 0*/, uint32_t start_instance_location /*= 0*/)
	{
		PrepareDraw();

		m_CommandList->IASetVertexBuffers(0, 1, &m_BoundVertexBuffer);
		m_CommandList->DrawInstanced(vertex_count, instance_count, start_vertex_location, start_instance_location);
	}

	void D3D12RHI::DrawIndexed(uint32_t index_count, uint32_t instance_count /*= 1*/, uint32_t start_index_location /*= 0*/, uint32_t base_vertex_location /*= 0*/, uint32_t start_instance_location /*= 0*/)
	{
		PrepareDraw();

		m_CommandList->IASetVertexBuffers(0, 1, &m_BoundVertexBuffer);
		m_CommandList->IASetIndexBuffer(&m_BoundIndexBuffer);
		m_CommandList->DrawIndexedInstanced(index_count, instance_count, start_index_location, base_vertex_location, start_instance_location);
	}

	void D3D12RHI::Dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
	{
		m_CommandList->Dispatch(group_count_x, group_count_y, group_count_z);
	}

	void D3D12RHI::GetHardwareAdapter()
	{
		m_Adapter = nullptr;

		ComPtr<IDXGIAdapter1> adapter;

		// Get IDXGIFactory6
		ComPtr<IDXGIFactory6> factory6;
		if (!FAILED(m_Factory->QueryInterface(IID_PPV_ARGS(&factory6))))
		{
			for (uint32_t adapter_index = 0;
				!FAILED(factory6->EnumAdapterByGpuPreference(adapter_index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)));
				 adapter_index++)
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
			ED_ASSERT_MB(false, "Failed to get IDXGIFactory6!");
		}

		ED_ASSERT_MB(adapter.Get() != nullptr, "Failed to get a compatible adapter!");

		 adapter.Swap(m_Adapter);
	}

	void D3D12RHI::Render()
	{
		ED_PROFILE_FUNCTION();
		ED_PROFILE_GPU_CONTEXT(m_CommandList.Get());
		ED_PROFILE_GPU_FUNCTION("D3D12RHI::Render");

		if (FAILED(m_CommandList->Close()))
			ED_ASSERT_MB(false, "Failed to close command list");

		ID3D12CommandList* command_lists[] = { m_CommandList.Get() };
		m_CommandQueue->ExecuteCommandLists(ARRAYSIZE(command_lists), command_lists);

		// Schedule a Signal command in the queue
		const uint64_t current_fence_value = m_FenceValues[m_FrameIndex];
		m_CommandQueue->Signal(m_Fence.Get(), current_fence_value);

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
		m_FenceValues[m_FrameIndex] = current_fence_value + 1;

		WaitForGPU();

		m_CommandAllocator->Reset();

		m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);
	}

	void D3D12RHI::Resize(uint32_t width, uint32_t height)
	{
		ED_ASSERT_LOG(m_SwapchainTarget, "Failed to resize, no Render Target Render Pass assigned");

		auto internal_state = ToInternal(m_SwapchainTarget.get());
		if (m_Device && m_Swapchain)
		{
			ED_LOG_TRACE("Resizing Window to {}x{}", width, height);

			WaitForGPU();
			if (FAILED(m_CommandList->Close()))
				ED_ASSERT_MB(false, "Failed to close command list");

			for (size_t i = 0; i < s_FrameCount; ++i)
			{
				auto rt_internal_state = ToInternal(m_SwapchainTarget->color_attachments[i].get());
				rt_internal_state->resource.Reset();
				m_FenceValues[i] = m_FenceValues[m_FrameIndex];
			}
			
			m_SwapchainTarget->depth_stencil.reset();

			m_Swapchain->ResizeBuffers(s_FrameCount, width, height, DXGI_FORMAT_UNKNOWN, 0);

			m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

			m_SwapchainTarget->color_attachments.clear();
			m_SwapchainTarget->desc.width = width;
			m_SwapchainTarget->desc.height = height;
			CreateAttachments(m_SwapchainTarget);
			
			m_Scissor  = CD3DX12_RECT(0, 0, width, height);

			m_CommandAllocator->Reset();
			m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);
		}
	}

	void D3D12RHI::ChangeResourceState(std::shared_ptr<Texture>& resource, ResourceState current_state, ResourceState desired_state, int subresource /*= -1*/)
	{
		auto internal_state = ToInternal(resource.get());
		uint32_t sub = subresource == -1 ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : (uint32_t)subresource;

		m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
												internal_state->resource.Get(), 
												Helpers::ConvertResourceState(current_state),
												Helpers::ConvertResourceState(desired_state),
												sub));
	}

	D3D12_GPU_DESCRIPTOR_HANDLE D3D12RHI::GetGPUHandle(std::shared_ptr<DescriptorHeap> descriptor_heap, D3D12_DESCRIPTOR_HEAP_TYPE heap_type /*= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV*/, int32_t offset /*= 0*/)
	{
		CD3DX12_GPU_DESCRIPTOR_HANDLE handle(descriptor_heap->heap->GetGPUDescriptorHandleForHeapStart());
		handle.Offset(offset, m_Device->GetDescriptorHandleIncrementSize(heap_type));

		return handle;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12RHI::GetCPUHandle(std::shared_ptr<DescriptorHeap> descriptor_heap, D3D12_DESCRIPTOR_HEAP_TYPE heap_type /*= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV*/, int32_t offset /*= 0*/)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(descriptor_heap->heap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(offset, m_Device->GetDescriptorHandleIncrementSize(heap_type));

		return handle;
	}

	uint32_t D3D12RHI::AllocateHandle(std::shared_ptr<DescriptorHeap> descriptor_heap, D3D12_DESCRIPTOR_HEAP_TYPE heap_type /*= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV*/)
	{
		if (heap_type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
			ED_ASSERT_LOG(descriptor_heap->offset < s_SRVDescriptorCount, "This SRV Descriptor heap is full!");

		auto handle_offset = descriptor_heap->offset;
		descriptor_heap->offset++;
		return handle_offset;
	}

	std::shared_ptr<DescriptorHeap> D3D12RHI::CreateDescriptorHeap(uint32_t num_descriptors, D3D12_DESCRIPTOR_HEAP_TYPE descriptor_type, D3D12_DESCRIPTOR_HEAP_FLAGS flags /*= D3D12_DESCRIPTOR_HEAP_FLAG_NONE*/)
	{
		std::shared_ptr<DescriptorHeap> descriptor_heap = std::make_shared<DescriptorHeap>();

		D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
		heap_desc.NumDescriptors = num_descriptors;
		heap_desc.Type = descriptor_type;
		heap_desc.Flags = flags;

		if (FAILED(m_Device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&descriptor_heap->heap))))
			ED_ASSERT_MB(false, "Failed to create Shader Resource View descriptor heap!");

		return descriptor_heap;
	}

	void D3D12RHI::SetDescriptorHeap(std::shared_ptr<DescriptorHeap> descriptor_heap)
	{
		// Right now this is ok to use because we are not using more than one desriptor heap
		ID3D12DescriptorHeap* heaps[] = { descriptor_heap->heap.Get() };
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

	uint32_t D3D12RHI::GetRootParameterIndex(const std::string& parameter_name)
	{
		auto root_parameter_index = m_BoundPipeline->root_parameter_indices.find(parameter_name.data());

		bool root_parameter_found = root_parameter_index != m_BoundPipeline->root_parameter_indices.end();
		ED_ASSERT_LOG(root_parameter_found, "Failed to find root parameter!");

		return root_parameter_index->second;
	}
}
