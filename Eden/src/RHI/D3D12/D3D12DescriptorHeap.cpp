#include "D3D12DescriptorHeap.h"

namespace Eden
{
	D3D12DescriptorHeap::D3D12DescriptorHeap(ID3D12Device* device, uint32_t numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc		= {};
		heapDesc.NumDescriptors					= numDescriptors;
		heapDesc.Type							= type;
		heapDesc.Flags							= flags;
		device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_Heap));

		m_IncrementSize = device->GetDescriptorHandleIncrementSize(type);
	}

	uint32_t D3D12DescriptorHeap::AllocateHandle()
	{
		uint32_t index = m_CurrentIndex;
		m_CurrentIndex++;
		return index;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorHeap::GetCPUHandle(uint32_t index)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_Heap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(index, m_IncrementSize);

		return handle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE D3D12DescriptorHeap::GetGPUHandle(uint32_t index)
	{
		CD3DX12_GPU_DESCRIPTOR_HANDLE handle(m_Heap->GetGPUDescriptorHandleForHeapStart());
		handle.Offset(index, m_IncrementSize);

		return handle;
	}

}