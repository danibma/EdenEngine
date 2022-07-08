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

namespace Eden
{
	// Internal Helpers
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

	constexpr D3D_FEATURE_LEVEL s_FeatureLevel = D3D_FEATURE_LEVEL_12_1;

	void D3D12RHI::Init(Window* window)
	{
		m_Scissor = CD3DX12_RECT(0, 0, window->GetWidth(), window->GetHeight());
		m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)window->GetWidth(), (float)window->GetHeight());
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

		// Describe and create command queue
		D3D12_COMMAND_QUEUE_DESC command_queue_desc = {};
		command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		if (FAILED(m_Device->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&m_CommandQueue))))
			ED_ASSERT_MB(false, "Failed to Create command queue!");

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
		m_SRVHeap = CreateDescriptorHeap(128, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

		//CreateBackBuffers( window->GetWidth(), window->GetHeight());

		// Create command allocator
		if (FAILED(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator))))
		   ED_ASSERT_MB(false, "Failed to create command allocator!");

		m_CommandAllocator->SetName(L"gfx_command_allocator");

		// Create command list
		if (FAILED(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_CommandList))))
			ED_ASSERT_MB(false, "Failed to create command list!");

		m_CommandList->SetName(L"gfx_command_list");

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
		case Vertex:
			entry_point = L"VSMain";
			stage_str = L"vs_6_0";
			break;
		case Pixel:
			entry_point = L"PSMain";
			stage_str = L"ps_6_0";
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

	D3D12_STATIC_SAMPLER_DESC D3D12RHI::CreateStaticSamplerDesc(const uint32_t shader_register, const uint32_t register_space, const D3D12_SHADER_VISIBILITY shader_visibility)
	{
		D3D12_STATIC_SAMPLER_DESC sampler;
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
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
		
		uint32_t shader_count = 2; // For now this is always two because its only compiling vertex and pixel shader, in the future add a way to check this dynamically
		for (uint32_t i = 0; i < shader_count; ++i)
		{
			ComPtr<ID3D12ShaderReflection> reflection;
			D3D12_SHADER_VISIBILITY shader_visibility;

			// Get shader stage
			switch (i)
			{
			case ShaderStage::Vertex:
				reflection = internal_state->vertex_reflection;
				shader_visibility = D3D12_SHADER_VISIBILITY_VERTEX;
				break;
			case ShaderStage::Pixel:
				reflection = internal_state->pixel_reflection;
				shader_visibility = D3D12_SHADER_VISIBILITY_PIXEL;
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
						D3D12_STATIC_SAMPLER_DESC sampler = {};
						sampler.Filter = D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
						sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
						sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
						sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
						sampler.MipLODBias = 0;
						sampler.MaxAnisotropy = 0;
						sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
						sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
						sampler.MinLOD = 0.0f;
						sampler.MaxLOD = D3D12_FLOAT32_MAX;
						sampler.ShaderRegister = desc.BindPoint;
						sampler.RegisterSpace = desc.Space;
						sampler.ShaderVisibility = shader_visibility;
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

	std::shared_ptr<Eden::Buffer> D3D12RHI::CreateBuffer(BufferDesc* desc, const void* initial_data)
	{
		std::shared_ptr<Buffer> buffer = std::make_shared<Buffer>();
		auto internal_state = std::make_shared<D3D12Buffer>();
		buffer->internal_state = internal_state;
		buffer->desc = *desc;

		buffer->size = buffer->desc.element_count * buffer->desc.stride;
		if (buffer->desc.usage == BufferDesc::Uniform)
			buffer->size = ALIGN(buffer->size, 256);

		D3D12MA::ALLOCATION_DESC allocation_desc = {};
		allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

		m_Allocator->CreateResource(&allocation_desc,
									&CD3DX12_RESOURCE_DESC::Buffer(buffer->size),
									D3D12_RESOURCE_STATE_GENERIC_READ,
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

			internal_state->cpu_handle = GetCPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_SRVHeapOffset);
			m_Device->CreateShaderResourceView(internal_state->resource.Get(), &srv_desc, internal_state->cpu_handle);
			internal_state->gpu_handle = GetGPUHandle(m_SRVHeap, m_SRVHeapOffset);
			m_SRVHeapOffset++;
		}
		break;
		case BufferDesc::Uniform:
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
			cbv_desc.BufferLocation = internal_state->resource->GetGPUVirtualAddress();
			cbv_desc.SizeInBytes = buffer->size;

			internal_state->cpu_handle = GetCPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_SRVHeapOffset);
			m_Device->CreateConstantBufferView(&cbv_desc, internal_state->cpu_handle);
			internal_state->gpu_handle = GetGPUHandle(m_SRVHeap, m_SRVHeapOffset);
			m_SRVHeapOffset++;
		}
		break;
		}

		return buffer;
	}

	void D3D12RHI::CreateBackBuffers(std::shared_ptr<RenderPass>& render_pass, uint32_t width, uint32_t height)
	{
		auto internal_state = ToInternal(render_pass.get());

		// Create render targets
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(GetCPUHandle(internal_state->rtv_heap));

		if (render_pass->desc.usage == RenderPassDesc::RenderTexture)
		{
			auto rt_internal_state = ToInternal(render_pass->render_targets[0].get());

			auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			CD3DX12_CPU_DESCRIPTOR_HANDLE srv_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(GetCPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_SRVHeapOffset));

			D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(Helpers::ConvertFormat(Format::RGBA32_FLOAT),
																	width, height,
																	1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

			D3D12_CLEAR_VALUE clear_value = { Helpers::ConvertFormat(Format::RGBA32_FLOAT), { 0.f, 0.f, 0.f, 0.f } };

			m_Device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
											  &desc,
											  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear_value,
											  IID_PPV_ARGS(rt_internal_state->resource.ReleaseAndGetAddressOf()));

			m_Device->CreateRenderTargetView(rt_internal_state->resource.Get(), nullptr, rtv_handle);

			rt_internal_state->cpu_handle = srv_handle;
			rt_internal_state->gpu_handle = GetGPUHandle(m_SRVHeap, m_SRVHeapOffset);
			render_pass->render_targets[0]->desc.width = width;
			render_pass->render_targets[0]->desc.height = height;
			m_Device->CreateShaderResourceView(rt_internal_state->resource.Get(), nullptr, srv_handle);
			m_SRVHeapOffset++;
		}
		else if (render_pass->desc.usage == RenderPassDesc::RenderTarget)
		{

			// Create a RTV for each frame
			for (uint32_t i = 0; i < s_FrameCount; ++i)
			{
				auto rt_internal_state = ToInternal(render_pass->render_targets[i].get());

				if (FAILED(m_Swapchain->GetBuffer(i, IID_PPV_ARGS(&rt_internal_state->resource))))
					ED_ASSERT_MB(false, "Failed to get render target from swapchain!");

				m_Device->CreateRenderTargetView(rt_internal_state->resource.Get(), nullptr, rtv_handle);
				rtv_handle.Offset(1, m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

				std::wstring rt_name;
				render_pass->desc.debug_name += "backbuffer";
				Utils::StringConvert(render_pass->desc.debug_name, rt_name);;
				rt_internal_state->resource->SetName(rt_name.c_str());
				render_pass->render_targets[i]->desc.width = width;
				render_pass->render_targets[i]->desc.height = height;
			}
		}

		render_pass->width = width;
		render_pass->height = height;

		// Create DSV
		D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_desc = {};
		depth_stencil_desc.Format = DXGI_FORMAT_D32_FLOAT;
		depth_stencil_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		depth_stencil_desc.Flags = D3D12_DSV_FLAG_NONE;

		D3D12_CLEAR_VALUE depth_optimized_clear_value;
		depth_optimized_clear_value.Format = DXGI_FORMAT_D32_FLOAT;
		depth_optimized_clear_value.DepthStencil.Depth = 1.0f;
		depth_optimized_clear_value.DepthStencil.Stencil = 0;

		if (FAILED(m_Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depth_optimized_clear_value,
			IID_PPV_ARGS(&internal_state->depth_stencil)
		)))
		{
			ED_ASSERT_MB(false, "Failed to create depth stencil buffer");
		}

		auto dsv_handle = GetCPUHandle(internal_state->dsv_heap);
		m_Device->CreateDepthStencilView(internal_state->depth_stencil.Get(), &depth_stencil_desc, dsv_handle);
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

		// Get shader file path
		std::wstring shader_path = L"shaders/";
		shader_path.append(std::wstring(desc->program_name.begin(), desc->program_name.end()));
		shader_path.append(L".hlsl");

		auto vertex_shader = CompileShader(shader_path, ShaderStage::Vertex);
		internal_state->vertex_reflection = vertex_shader.reflection;
		auto pixel_shader = CompileShader(shader_path, ShaderStage::Pixel);
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
		pso_desc.BlendState.RenderTarget[0] = render_target_blend_desc;
		pso_desc.SampleMask = UINT_MAX;
		pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pso_desc.RasterizerState.CullMode = Helpers::ConvertCullMode(desc->cull_mode);
		pso_desc.RasterizerState.FrontCounterClockwise = desc->front_counter_clockwise;
		pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pso_desc.DepthStencilState.DepthFunc = Helpers::ConvertComparisonFunc(desc->depth_func);
		pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		pso_desc.InputLayout.NumElements = static_cast<uint32_t>(input_elements.size());
		pso_desc.InputLayout.pInputElementDescs = input_elements.data();
		pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pso_desc.NumRenderTargets = 1;
		pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pso_desc.SampleDesc.Count = 1;

		if (FAILED(m_Device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&internal_state->pipeline_state))))
			ED_ASSERT_MB(false, "Failed to create graphics pipeline state!");

		std::wstring pipeline_name;
		Utils::StringConvert(desc->program_name, pipeline_name);
		pipeline_name.append(L"_PSO");
		internal_state->pipeline_state->SetName(pipeline_name.c_str());

		return pipeline;
	}

	std::shared_ptr<Texture> D3D12RHI::CreateTexture(std::string file_path)
	{
		ED_PROFILE_FUNCTION();

		TextureDesc desc;
		if (stbi_is_hdr(file_path.c_str()))
			desc.srgb = true;

		// Load texture from file
		int width, height, nchannels;
		unsigned char* texture_file = stbi_load(file_path.c_str(), &width, &height, &nchannels, 4);
		if (!texture_file)
		{
			ED_LOG_ERROR("Could not load texture file: {}", file_path);
			return nullptr;
		}

		desc.data = texture_file;
		desc.width = width;
		desc.height = height;

		std::shared_ptr<Texture> texture = CreateTexture(&desc);
		
		return texture;
	}

	std::shared_ptr<Texture> D3D12RHI::CreateTexture(TextureDesc* desc)
	{
		ED_PROFILE_FUNCTION();

		ED_ASSERT_LOG(desc->usage == TextureDesc::Texture2D, "Only texture2d is implemented!");

		std::shared_ptr<Texture> texture = std::make_shared<Texture>();
		auto internal_state = std::make_shared<D3D12Texture>();
		texture->internal_state = internal_state;
		texture->desc = *desc;
		texture->image_format = texture->desc.srgb ? Format::SRGB : Format::RGBA8_UNORM;

		ComPtr<ID3D12Resource> texture_upload_heap;
		D3D12MA::Allocation* upload_allocation;

		ED_PROFILE_GPU_CONTEXT(m_CommandList.Get());
		ED_PROFILE_GPU_FUNCTION("D3D12RHI::CreateTexture");

		// Create the texture
		{
			// Describe and create a texture2D
			D3D12_RESOURCE_DESC texture_desc = {};
			texture_desc.MipLevels = 1;
			texture_desc.Format = Helpers::ConvertFormat(texture->image_format);
			texture_desc.Width = texture->desc.width;
			texture_desc.Height = texture->desc.height;
			texture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
			texture_desc.DepthOrArraySize = 1;
			texture_desc.SampleDesc.Count = 1;
			texture_desc.SampleDesc.Quality = 0;
			texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			D3D12MA::ALLOCATION_DESC allocation_desc = {};
			allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

			m_Allocator->CreateResource(&allocation_desc,
										&texture_desc,
										D3D12_RESOURCE_STATE_COPY_DEST,
										nullptr,
										&internal_state->allocation,
										IID_PPV_ARGS(&internal_state->resource));

			const uint64_t upload_buffer_size = GetRequiredIntermediateSize(internal_state->resource.Get(), 0, 1);

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
			D3D12_SUBRESOURCE_DATA texture_data = {};
			texture_data.pData = texture->desc.data;
			texture_data.RowPitch = texture->desc.width * 4;
			texture_data.SlicePitch = texture->desc.height * texture_data.RowPitch;

			UpdateSubresources(m_CommandList.Get(), internal_state->resource.Get(), texture_upload_heap.Get(), 0, 0, 1, &texture_data);
			ChangeResourceState(internal_state->resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}

		m_CommandList->Close();
		ID3D12CommandList* command_lists[] = { m_CommandList.Get() };
		m_CommandQueue->ExecuteCommandLists(_countof(command_lists), command_lists);
		m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);

		// Describe and create a Shader resource view(SRV) for the texture
		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Format = Helpers::ConvertFormat(texture->image_format);
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;

		internal_state->cpu_handle = GetCPUHandle(m_SRVHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_SRVHeapOffset);
		m_Device->CreateShaderResourceView(internal_state->resource.Get(), &srv_desc, internal_state->cpu_handle);
		internal_state->gpu_handle = GetGPUHandle(m_SRVHeap, m_SRVHeapOffset);

		WaitForGPU();

		upload_allocation->Release();

		m_SRVHeapOffset++;

		// Enable the srv heap again
		if (m_SRVHeap->is_set)
		{
			SetDescriptorHeap(m_SRVHeap);
		}

		return texture;
	}

	std::shared_ptr<RenderPass> D3D12RHI::CreateRenderPass(RenderPassDesc* desc)
	{
		std::shared_ptr<RenderPass> render_pass = std::make_shared<RenderPass>();
		auto internal_state = std::make_shared<D3D12RenderPass>();
		render_pass->internal_state = internal_state;
		render_pass->desc = *desc;
		render_pass->width = static_cast<uint32_t>(m_Viewport.Width);
		render_pass->height = static_cast<uint32_t>(m_Viewport.Height);

		internal_state->rtv_heap = CreateDescriptorHeap(s_FrameCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
		internal_state->dsv_heap = CreateDescriptorHeap(1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
		auto rtv_handle = GetCPUHandle(internal_state->rtv_heap, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 0);
		auto dsv_handle = GetCPUHandle(internal_state->dsv_heap, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 0);

		for (auto& render_target : render_pass->render_targets)
		{
			render_target = std::make_shared<Texture>();
			render_target->internal_state = std::make_shared<D3D12Texture>();
		}

		CreateBackBuffers(render_pass, render_pass->width, render_pass->height);

		return render_pass;
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
		ImGui_ImplDX12_Init(m_Device.Get(), s_FrameCount, DXGI_FORMAT_R8G8B8A8_UNORM, m_SRVHeap->heap.Get(), m_SRVHeap->heap->GetCPUDescriptorHandleForHeapStart(), m_SRVHeap->heap->GetGPUDescriptorHandleForHeapStart());
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

		m_CommandList->SetPipelineState(internal_state->pipeline_state.Get());
		m_CommandList->SetGraphicsRootSignature(internal_state->root_signature.Get());
		m_BoundPipeline = pipeline;
	}

	void D3D12RHI::BindVertexBuffer(std::shared_ptr<Buffer>& vertex_buffer)
	{
		auto internal_state = ToInternal(vertex_buffer.get());

		ED_ASSERT_LOG(internal_state->resource != nullptr, "Can't bind a empty vertex buffer!");
		m_BoundVertexBuffer.BufferLocation	= internal_state->resource->GetGPUVirtualAddress();
		m_BoundVertexBuffer.SizeInBytes		= vertex_buffer->size;
		m_BoundVertexBuffer.StrideInBytes	= vertex_buffer->desc.stride;
	}

	void D3D12RHI::BindIndexBuffer(std::shared_ptr<Buffer>& index_buffer)
	{
		auto internal_state = ToInternal(index_buffer.get());

		ED_ASSERT_LOG(internal_state->resource != nullptr, "Can't bind a empty index buffer!");
		m_BoundIndexBuffer.BufferLocation	= internal_state->resource->GetGPUVirtualAddress();
		m_BoundIndexBuffer.SizeInBytes		= index_buffer->size;
		m_BoundIndexBuffer.Format			= DXGI_FORMAT_R32_UINT;
	}

	void D3D12RHI::BindParameter(const std::string& parameter_name, const std::shared_ptr<Buffer>& buffer)
	{
		auto internal_state = ToInternal(buffer.get());

		uint32_t root_parameter_index = GetRootParameterIndex(parameter_name);
		m_CommandList->SetGraphicsRootDescriptorTable(root_parameter_index, internal_state->gpu_handle);
	}

	void D3D12RHI::BindParameter(const std::string& parameter_name, const std::shared_ptr<Texture>& texture)
	{
		auto internal_state = ToInternal(texture.get());

		uint32_t root_parameter_index = GetRootParameterIndex(parameter_name);
		m_CommandList->SetGraphicsRootDescriptorTable(root_parameter_index, internal_state->gpu_handle);
	}

	void D3D12RHI::BeginRender()
	{
		// Set SRV Descriptor heap
		SetDescriptorHeap(m_SRVHeap);
	}

	void D3D12RHI::BeginRenderPass(std::shared_ptr<RenderPass>& render_pass)
	{
		auto internal_state = ToInternal(render_pass.get());

		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
		D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = GetCPUHandle(internal_state->dsv_heap, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 0);
		if (render_pass->desc.usage ==RenderPassDesc::RenderTarget)
		{
			auto current_render_target = GetCurrentRenderTarget(render_pass);
			rtv_handle = GetCPUHandle(internal_state->rtv_heap, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_FrameIndex);
			ChangeResourceState(current_render_target, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
		else if (render_pass->desc.usage ==RenderPassDesc::RenderTexture)
		{
			// Check if the render targets need to get resized
			if (render_pass->width != render_pass->render_targets[0]->desc.width ||
				render_pass->height != render_pass->render_targets[0]->desc.height)
			{
				ResizeRenderPassTexture(render_pass, render_pass->width, render_pass->height);
			}

			auto rt_internal_state = ToInternal(render_pass->render_targets[0].get());
			auto current_render_target = rt_internal_state->resource.Get();
			rtv_handle = GetCPUHandle(internal_state->rtv_heap, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 0);
			ChangeResourceState(current_render_target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
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
		dsv_clear.ClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		dsv_clear.ClearValue.DepthStencil.Depth = 1.0f;
		D3D12_RENDER_PASS_BEGINNING_ACCESS dsv_begin_access{ begin_access_type, { dsv_clear } };
		D3D12_RENDER_PASS_ENDING_ACCESS dsv_end_access{ end_access_type, {} };
		internal_state->dsv_desc = { dsv_handle, dsv_begin_access, dsv_begin_access, dsv_end_access, dsv_end_access };

		m_CommandList->BeginRenderPass(1, &internal_state->rtv_desc, &internal_state->dsv_desc, D3D12_RENDER_PASS_FLAG_NONE);
	}

	void D3D12RHI::SetRTRenderPass(std::shared_ptr<RenderPass>& render_pass)
	{
		m_RTRenderPass = render_pass;
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

		if (render_pass->desc.usage == RenderPassDesc::RenderTarget)
		{
			auto current_render_target = GetCurrentRenderTarget(render_pass);
			ChangeResourceState(current_render_target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		}
		else if (render_pass->desc.usage == RenderPassDesc::RenderTexture)
		{
			auto rt_internal_state = ToInternal(render_pass->render_targets[0].get());
			auto current_render_target = rt_internal_state->resource.Get();
			ChangeResourceState(current_render_target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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

		m_Viewport.MinDepth = m_BoundPipeline->desc.min_depth;
		m_CommandList->RSSetViewports(1, &m_Viewport);
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
		ED_ASSERT_LOG(m_RTRenderPass, "Failed to resize, no Render Target Render Pass assigned");

		auto internal_state = ToInternal(m_RTRenderPass.get());
		if (m_Device && m_Swapchain)
		{
			ED_LOG_TRACE("Resizing Window to {}x{}", width, height);

			WaitForGPU();
			if (FAILED(m_CommandList->Close()))
				ED_ASSERT_MB(false, "Failed to close command list");

			for (size_t i = 0; i < s_FrameCount; ++i)
			{
				auto rt_internal_state = ToInternal(m_RTRenderPass->render_targets[i].get());
				rt_internal_state->resource.Reset();
				m_FenceValues[i] = m_FenceValues[m_FrameIndex];
			}
			
			internal_state->depth_stencil.Reset();

			m_Swapchain->ResizeBuffers(s_FrameCount, width, height, DXGI_FORMAT_UNKNOWN, 0);

			m_FrameIndex = m_Swapchain->GetCurrentBackBufferIndex();

			CreateBackBuffers(m_RTRenderPass, width, height);

			m_RTRenderPass->width = width;
			m_RTRenderPass->height = height;

			m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)width, (float)height);
			m_Scissor  = CD3DX12_RECT(0, 0, width, height);

			m_CommandAllocator->Reset();
			m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);
		}
	}

	void D3D12RHI::ResizeRenderPassTexture(std::shared_ptr<RenderPass>& render_pass, uint32_t width, uint32_t height)
	{
		ED_ASSERT_LOG(render_pass->desc.usage == RenderPassDesc::RenderTexture, "Render pass cannot be resized because it isn't a render texture");

		auto internal_state = ToInternal(render_pass.get());
		auto rt_internal_state = ToInternal(render_pass->render_targets[0].get());

		auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(GetCPUHandle(internal_state->rtv_heap));

		D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM,
																width, height,
																1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		D3D12_CLEAR_VALUE clear_value = { DXGI_FORMAT_R8G8B8A8_UNORM, { 0.f, 0.f, 0.f, 0.f } };

		m_Device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
										  &desc,
										  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear_value,
										  IID_PPV_ARGS(rt_internal_state->resource.ReleaseAndGetAddressOf()));

		m_Device->CreateRenderTargetView(rt_internal_state->resource.Get(), nullptr, rtv_handle);

		render_pass->render_targets[0]->desc.width = width;
		render_pass->render_targets[0]->desc.height = height;
		m_Viewport.Width  = static_cast<float>(width);
		m_Viewport.Height = static_cast<float>(height);
		m_Device->CreateShaderResourceView(rt_internal_state->resource.Get(), nullptr, rt_internal_state->cpu_handle);

		// Create DSV
		D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_desc = {};
		depth_stencil_desc.Format = DXGI_FORMAT_D32_FLOAT;
		depth_stencil_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		depth_stencil_desc.Flags = D3D12_DSV_FLAG_NONE;

		D3D12_CLEAR_VALUE depth_optimized_clear_value;
		depth_optimized_clear_value.Format = DXGI_FORMAT_D32_FLOAT;
		depth_optimized_clear_value.DepthStencil.Depth = 1.0f;
		depth_optimized_clear_value.DepthStencil.Stencil = 0;

		if (FAILED(m_Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depth_optimized_clear_value,
			IID_PPV_ARGS(&internal_state->depth_stencil)
		)))
		{
			ED_ASSERT_MB(false, "Failed to create depth stencil buffer");
		}

		auto dsv_handle = GetCPUHandle(internal_state->dsv_heap);
		m_Device->CreateDepthStencilView(internal_state->depth_stencil.Get(), &depth_stencil_desc, dsv_handle);
	}

	ID3D12Resource* D3D12RHI::GetCurrentRenderTarget(std::shared_ptr<RenderPass>& render_pass)
	{
		auto internal_state = ToInternal(render_pass->render_targets[m_FrameIndex].get());
		return internal_state->resource.Get();
	}

	void D3D12RHI::ChangeResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES current_state, D3D12_RESOURCE_STATES desired_state)
	{
		ED_PROFILE_GPU_CONTEXT(m_CommandList.Get());
		ED_PROFILE_GPU_FUNCTION("D3D12RHI::ChangeResourceState");
		m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(resource, current_state, desired_state));
	}

	D3D12_GPU_DESCRIPTOR_HANDLE D3D12RHI::GetGPUHandle(std::shared_ptr<DescriptorHeap> descriptor_heap, int32_t offset /*= 0*/)
	{
		CD3DX12_GPU_DESCRIPTOR_HANDLE handle(descriptor_heap->heap->GetGPUDescriptorHandleForHeapStart());
		handle.Offset(offset, m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		return handle;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12RHI::GetCPUHandle(std::shared_ptr<DescriptorHeap> descriptor_heap, D3D12_DESCRIPTOR_HEAP_TYPE heap_type /*= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV*/, int32_t offset /*= 0*/)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(descriptor_heap->heap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(offset, m_Device->GetDescriptorHandleIncrementSize(heap_type));

		return handle;
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
		descriptor_heap->is_set = true;
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
