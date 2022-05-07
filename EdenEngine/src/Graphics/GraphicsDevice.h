#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl\client.h>

#include "Core/Base.h"

using namespace Microsoft::WRL;

namespace Eden
{
	class GraphicsDevice
	{
		ComPtr<ID3D12Device> m_Device;
		ComPtr<IDXGIFactory4> m_Factory;
		ComPtr<IDXGIAdapter1> m_Adapter;

	public:
		GraphicsDevice();
		~GraphicsDevice();

	private:
		void GetHardwareAdapter();
	};
}

