#include "Memory.h"
#include "Core/Log.h"
#include "Core/Assertions.h"

#define UNKNOW_MEMORY_SOURCE "Unknown"

namespace Eden::Memory
{
	static bool s_bIsInInit = false;

	void MemoryManager::Init()
	{
		if (s_Data)
			return;

		s_bIsInInit = true;
		MemoryManagerData* data = (MemoryManagerData*)malloc(sizeof(MemoryManagerData));
		new(data) MemoryManagerData();
		s_Data = data;
		s_bIsInInit = false;
	}

	void* MemoryManager::Allocate(size_t size)
	{
		if (s_bIsInInit)
			return malloc(size);

		if (!s_Data)
			Init();

		void* memory = malloc(size);

		if (s_Data)
		{
			Allocation& allocation = s_Data->AllocationsMap[memory];
			allocation.memory = memory;
			allocation.size = size;
			allocation.source = UNKNOW_MEMORY_SOURCE;
			s_Data->AllocationStatsMap[UNKNOW_MEMORY_SOURCE].total_allocated += size;
		}

		return memory;
	}

	void* MemoryManager::Allocate(size_t size, const char* source)
	{
		void* memory = malloc(size);

		Allocation& allocation = s_Data->AllocationsMap[memory];
		allocation.memory = memory;
		allocation.size = size;
		allocation.source = source;
		s_Data->AllocationStatsMap[source].total_allocated += size;

		return memory;
	}

	void MemoryManager::Free(void* memory)
	{
		ensureMsg(s_Data->AllocationsMap.find(memory) != s_Data->AllocationsMap.end(), "This block of memory wasn't found!");

		if (s_Data)
		{
			Allocation& allocation = s_Data->AllocationsMap[memory];
			s_Data->AllocationStatsMap[allocation.source].total_freed += allocation.size;
			s_Data->AllocationsMap.erase(memory);
		}
		
		free(memory);
	}

	size_t MemoryManager::GetTotalAllocated()
	{
		size_t total = 0;
		for (const auto& allocStats : s_Data->AllocationStatsMap)
		{
			total += allocStats.second.total_allocated;
		}

		return total;
	}

	size_t MemoryManager::GetTotalFreed()
	{
		size_t total = 0;
		for (const auto& allocStats : s_Data->AllocationStatsMap)
		{
			total += allocStats.second.total_freed;
		}

		return total;
	}

	size_t MemoryManager::GetCurrentAllocated()
	{
		return GetTotalAllocated() - GetTotalFreed();
	}

	std::map<size_t, const char*, std::greater<size_t>> MemoryManager::GetCurrentAllocatedSources()
	{
		std::map<size_t, const char*, std::greater<size_t>> result;
		for (const auto& allocStats : s_Data->AllocationStatsMap)
		{
			size_t currentUsage = allocStats.second.total_allocated - allocStats.second.total_freed;
			result.emplace(currentUsage, allocStats.first);
		}

		return result;
	}

}

#ifdef ED_TRACK_MEMORY

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size)
{
	return Eden::Memory::MemoryManager::Allocate(size);
}

void* __CRTDECL operator new(size_t size, const char* source)
{
	return Eden::Memory::MemoryManager::Allocate(size, source);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size)
{
	return Eden::Memory::MemoryManager::Allocate(size);
}

void* __CRTDECL operator new[](size_t size, const char* source)
{
	return Eden::Memory::MemoryManager::Allocate(size, source);
}

void __CRTDECL operator delete(void* memory)
{
	return Eden::Memory::MemoryManager::Free(memory);
}

void __CRTDECL operator delete(void* memory, const char* source)
{
	return Eden::Memory::MemoryManager::Free(memory);
}

void __CRTDECL operator delete[](void* memory)
{
	return Eden::Memory::MemoryManager::Free(memory);
}

void __CRTDECL operator delete[](void* memory, const char* source)
{
	return Eden::Memory::MemoryManager::Free(memory);
}

#endif // ED_TRACK_MEMORY