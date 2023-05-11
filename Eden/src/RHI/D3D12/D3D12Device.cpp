#include "D3D12Device.h"
#include "Core/Assertions.h"
#include "Utilities/Utils.h"
#include "Core/Memory/Memory.h"

#include "D3D12DescriptorHeap.h"

#include <dxgidebug.h>
#include <dxgi1_6.h>

namespace Eden
{

	D3D12Device::D3D12Device()
	{
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
			ensureMsg(false, "Failed to create dxgi factory!");

		GetHardwareAdapter();

		if (FAILED(D3D12CreateDevice(m_Adapter, GFeatureLevel, IID_PPV_ARGS(&m_Device))))
			ensureMsg(false, "Faile to create device");

#ifdef ED_DEBUG
		{
			if (!FAILED(m_Device->QueryInterface(IID_PPV_ARGS(&m_InfoQueue))))
			{
				ComPtr<ID3D12InfoQueue1> infoQueue1;

				if (!FAILED(m_InfoQueue->QueryInterface(IID_PPV_ARGS(&infoQueue1))))
					infoQueue1->RegisterMessageCallback(D3D12DebugMessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &m_MessageCallbackCookie);
			}
		}
#endif

		m_Device->SetName(L"gfxDevice");

		// Describe and create a shader resource view (SRV) heap for the texture.
		m_SRVHeap = enew D3D12DescriptorHeap(m_Device, GSRVDescriptorCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
		m_RTVHeap = enew D3D12DescriptorHeap(m_Device, GRTVDescriptorCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
		m_DSVHeap = enew D3D12DescriptorHeap(m_Device, GDSVDescriptorCount, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

		DXGI_ADAPTER_DESC1 adapterDesc;
		m_Adapter->GetDesc1(&adapterDesc);
		std::string name;
		Utils::StringConvert(adapterDesc.Description, name);
		ED_LOG_INFO("Initialized D3D12 device on {}", name);

		// Create allocator
		D3D12MA::ALLOCATOR_DESC allocatorDesc	= {};
		allocatorDesc.pDevice					= m_Device;
		allocatorDesc.pAdapter					= m_Adapter;

		if (FAILED(D3D12MA::CreateAllocator(&allocatorDesc, &m_Allocator)))
			ensureMsg(false, "Failed to create allocator!");
	}

	D3D12Device::~D3D12Device()
	{
		edelete m_SRVHeap;
		edelete m_RTVHeap;
		edelete m_DSVHeap;

#if ED_DEBUG
		SAFE_RELEASE(m_InfoQueue);
#endif
		SAFE_RELEASE(m_Device);
		SAFE_RELEASE(m_Factory);
		SAFE_RELEASE(m_Adapter);
		SAFE_RELEASE(m_Allocator);
	}

	bool D3D12Device::SupportsRootSignature1_1()
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(m_Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			ED_LOG_WARN("Root signature version 1.1 not available, switching to 1.0");
			return false;
		}

		return true;
	}

	void D3D12Device::GetHardwareAdapter()
	{
		m_Adapter = nullptr;

		// Get IDXGIFactory6
		ComPtr<IDXGIFactory6> factory6;
		if (!FAILED(m_Factory->QueryInterface(IID_PPV_ARGS(&factory6))))
		{
			for (uint32_t adapterIndex = 0;
					!FAILED(factory6->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_Adapter)));
					adapterIndex++)
			{
				DXGI_ADAPTER_DESC1 desc;
				m_Adapter->GetDesc1(&desc);

				// Don't select the Basic Render Driver adapter.
				if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
					continue;

				// Check to see whether the adapter supports Direct3D 12, but don't create the
				// actual device yet.
				if (SUCCEEDED(D3D12CreateDevice(m_Adapter, GFeatureLevel, _uuidof(ID3D12Device), nullptr)))
					break;
			}
		}
		else
		{
			ensureMsg(false, "Failed to get IDXGIFactory6!");
		}

		ensureMsg(m_Adapter != nullptr, "Failed to get a compatible adapter!");
	}
}
