#include "Memory.h"
#include "Utilities/Utils.h"
#include "Log.h"

#ifdef ED_TRACK_MEMORY

namespace Eden::Memory
{
	static AllocationStats s_AllocationData;

	[[nodiscard]] const AllocationStats& GetAllocationStats()
	{
		return s_AllocationData;
	}
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size)
{
	Eden::Memory::s_AllocationData.TotalAllocated += size;

	void* memory = malloc(size);
	return memory;
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, const char* file, int line)
{
	Eden::Memory::s_AllocationData.TotalAllocated += size;

	void* memory = malloc(size);
	return memory;
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size)
{
	Eden::Memory::s_AllocationData.TotalAllocated += size;

	void* memory = malloc(size);
	return memory;
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, const char* file, int line)
{
	Eden::Memory::s_AllocationData.TotalAllocated += size;

	void* memory = malloc(size);
	return memory;
}

void __CRTDECL operator delete(void* memory)
{
	size_t size = sizeof(memory);
	Eden::Memory::s_AllocationData.TotalFreed += size;

	free(memory);
}

void __CRTDECL operator delete(void* memory, const char* file, int line)
{
	size_t size = sizeof(memory);
	Eden::Memory::s_AllocationData.TotalFreed += size;

	free(memory);
}

void __CRTDECL operator delete[](void* memory)
{
	size_t size = sizeof(memory);
	Eden::Memory::s_AllocationData.TotalFreed += size;

	free(memory);
}

void __CRTDECL operator delete[](void* memory, const char* file, int line)
{
	size_t size = sizeof(memory);
	Eden::Memory::s_AllocationData.TotalFreed += size;

	free(memory);
}

#else

namespace Eden::Memory
{
	[[nodiscard]] const AllocationStats& GetAllocationStats()
	{
	}

	const void DumpAllocationStats()
	{
	}
}

#endif // ED_TRACK_MEMORY
