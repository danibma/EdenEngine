#include "Memory.h"
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
	Eden::Memory::s_AllocationData.total_allocated += size;

	// Allocate a few more bytes(8 in this case, because its always 64 bit systems) to store the size
	// of the memory being allocated
	size_t* memory = (size_t*)malloc(size + sizeof(size_t));
	memory[0] = size;

	return (void*)(&memory[1]);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size)
{
	Eden::Memory::s_AllocationData.total_allocated += size;

	// Allocate a few more bytes(8 in this case, because its always 64 bit systems) to store the size
	// of the memory being allocated
	size_t* memory = (size_t*)malloc(size + sizeof(size_t));
	memory[0] = size;

	return (void*)(&memory[1]);
}

void __CRTDECL operator delete(void* memory)
{
	// make the pointer the right type, then get the size of that memory block
	// by accessing the original beginning of it, which is the -1 position,
	// because when we allocate we return 8 bytes up front of what was allocated,
	// beucase we don't want to return the size too, so when the delete operator is called
	// the memory block starts 8 bytes up front aswell, so we want to retrieve the block
	// that was originally allocated alongside with the size
	size_t* ptr = (size_t*)memory;
	size_t size = ptr[-1];
	void* memory_to_free = (void*)(&ptr[-1]);
	Eden::Memory::s_AllocationData.total_freed += size;

	free(memory_to_free);
}

void __CRTDECL operator delete[](void* memory)
{
	// make the pointer the right type, then get the size of that memory block
	// by accessing the original beginning of it, which is the -1 position,
	// because when we allocate we return 8 bytes up front of what was allocated,
	// beucase we don't want to return the size too, so when the delete operator is called
	// the memory block starts 8 bytes up front aswell, so we want to retrieve the block
	// that was originally allocated alongside with the size
	size_t* ptr = (size_t*)memory;
	size_t size = ptr[-1];
	void* memory_to_free = (void*)(&ptr[-1]);
	Eden::Memory::s_AllocationData.total_freed += size;

	free(memory_to_free);
}

#else

namespace Eden::Memory
{
	[[nodiscard]] const AllocationStats& GetAllocationStats()
	{
	}
}

#endif // ED_TRACK_MEMORY
