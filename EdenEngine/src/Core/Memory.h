#pragma once

#include <vcruntime.h>

// Memory Tracking
namespace Eden::Memory
{
	struct AllocationStats
	{
		size_t total_allocated = 0;
		size_t total_freed = 0;
	};

	[[nodiscard]] const AllocationStats& GetAllocationStats();
}

#ifdef ED_TRACK_MEMORY

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size);

void __CRTDECL operator delete(void* memory);
void __CRTDECL operator delete[](void* memory);

#define enew new
#define edelete delete

#else

#define enew new
#define edelete delete

#endif // ED_TRACK_MEMORY

