#pragma once

#include <functional>

// Memory Tracking
// NOTE(Daniel): Right now its only tracking the memory, in the future make own allocator
namespace Eden::Memory
{
	struct AllocationStats
	{
		size_t TotalAllocated = 0;
		size_t TotalFreed = 0;
	};

	[[nodiscard]] const AllocationStats& GetAllocationStats();
}

#ifdef ED_TRACK_MEMORY

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, const char* file, int line);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, const char* file, int line);

void __CRTDECL operator delete(void* memory);
void __CRTDECL operator delete(void* memory, const char* file, int line);
void __CRTDECL operator delete[](void* memory);
void __CRTDECL operator delete[](void* memory, const char* file, int line);

#define enew new(__FILE__, __LINE__)
#define edelete delete

#else

#define enew new
#define edelete delete

#endif // ED_TRACK_MEMORY

