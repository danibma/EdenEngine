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

	const void DumpAllocationStats()
	{
		auto& stats = GetAllocationStats();

		ED_LOG_INFO("Memory Info:");
		ED_LOG_INFO("    Total Allocated: {}", Utils::BytesToString(stats.TotalAllocated));
		ED_LOG_INFO("    Total Freed: {}", Utils::BytesToString(stats.TotalFreed));
		ED_LOG_INFO("    Current Usage: {}", Utils::BytesToString(stats.TotalAllocated - stats.TotalFreed));
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
