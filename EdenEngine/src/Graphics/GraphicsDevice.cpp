#include "GraphicsDevice.h"

#include <cstdint>

namespace Eden
{
	constexpr D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_1;

	GraphicsDevice::GraphicsDevice()
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
			}
		}
	#endif

		if (FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_Factory))))
			ED_LOG_FATAL("Failed to create dxgi factory!");

		GetHardwareAdapter();

		if (FAILED(D3D12CreateDevice(m_Adapter.Get(), featureLevel, IID_PPV_ARGS(&m_Device))))
			ED_LOG_FATAL("Faile to create device");

		DXGI_ADAPTER_DESC1 adapterDesc;
		m_Adapter->GetDesc1(&adapterDesc);
		std::wstring wname = adapterDesc.Description;
		std::string name = std::string(wname.begin(), wname.end());
		ED_LOG_INFO("Initialized D3D12 device on {}", name);
	}

	GraphicsDevice::~GraphicsDevice()
	{
	}

	void GraphicsDevice::GetHardwareAdapter()
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
				if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), featureLevel, _uuidof(ID3D12Device), nullptr)))
					break;
			}
		}
		else
		{
			ED_LOG_FATAL("Failed to get IDXGIFactory6!");
		}

		ED_ASSERT_LOG(adapter.Get() != nullptr, "Failed to get a compatible adapter!");

		 adapter.Swap(m_Adapter);
	}

}
