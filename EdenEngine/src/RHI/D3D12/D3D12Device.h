#pragma once

#include "D3D12Definitions.h"
#include "RHI/RHIDefinitions.h"

namespace Eden
{
	class D3D12DescriptorHeap;
	class D3D12Device
	{
		DWORD					m_MessageCallbackCookie;
		ID3D12InfoQueue*		m_InfoQueue;
		ID3D12Device*			m_Device;
		IDXGIFactory4*			m_Factory;
		IDXGIAdapter1*			m_Adapter;
		D3D12DescriptorHeap*	m_SRVHeap;
		D3D12DescriptorHeap*	m_RTVHeap;
		D3D12DescriptorHeap*	m_DSVHeap;
		D3D12MA::Allocator*		m_Allocator;

	public:
		D3D12Device();
		~D3D12Device();

		ID3D12Device* GetDevice() { return m_Device; }
		IDXGIFactory4* GetFactory() { return m_Factory; }
		D3D12MA::Allocator* GetAllocator() { return m_Allocator; }

		bool SupportsRootSignature1_1();

		D3D12DescriptorHeap* GetSRVDescriptorHeap() { return m_SRVHeap; }
		D3D12DescriptorHeap* GetRTVDescriptorHeap() { return m_RTVHeap; }
		D3D12DescriptorHeap* GetDSVDescriptorHeap() { return m_DSVHeap; }

	private:
		void GetHardwareAdapter();
	};
}

