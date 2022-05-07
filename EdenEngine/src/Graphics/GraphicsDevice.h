#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl\client.h>

#include "d3dx12.h"
#include "Core/Base.h"

using namespace Microsoft::WRL;

namespace Eden
{
	constexpr uint32_t s_frameCount = 2;

	class GraphicsDevice
	{
		ComPtr<ID3D12Device> m_device;
		ComPtr<IDXGIFactory4> m_factory;
		ComPtr<IDXGIAdapter1> m_adapter;
		ComPtr<ID3D12CommandQueue> m_commandQueue;
		ComPtr<IDXGISwapChain3> m_swapchain;
		ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
		ComPtr<ID3D12Resource> m_renderTargets[s_frameCount];
		ComPtr<ID3D12CommandAllocator> m_commandAllocator;
		ComPtr<ID3D12GraphicsCommandList> m_commandList;
		ComPtr<ID3D12Fence> m_fence;
		ComPtr<ID3D12PipelineState> m_pipelineState;

		HANDLE m_fenceEvent;

		uint32_t m_frameIndex;
		uint32_t m_fenceValue;
		uint32_t m_rtvDescriptorSize;

	public:
		GraphicsDevice(HWND handle, uint32_t width, uint32_t height);
		~GraphicsDevice();

		void Render();

	private:
		void GetHardwareAdapter();
		void PopulateCommandList();
		void WaitForPreviousFrame();
	};
}

