#pragma once

#include "D3D12Definitions.h"

namespace Eden
{
	class D3D12DescriptorHeap
	{
		ComPtr<ID3D12DescriptorHeap>	m_Heap;
		uint32_t						m_CurrentIndex  = 0;
		uint32_t						m_IncrementSize = 0;

	public:
		D3D12DescriptorHeap(ID3D12Device* device, uint32_t numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags);

		uint32_t AllocateHandle();

		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index);
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t index);

		ID3D12DescriptorHeap* GetHeap() { return m_Heap.Get(); }
	};
}

