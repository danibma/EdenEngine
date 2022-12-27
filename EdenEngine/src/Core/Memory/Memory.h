#pragma once

#include <vcruntime.h>
#include <map>

// Memory Tracking
namespace Eden::Memory
{
	struct AllocationStats
	{
		size_t total_allocated = 0;
		size_t total_freed = 0;
	};

	struct Allocation
	{
		void* memory;
		size_t size;
		const char* source;
	};
	
	template <class T>
	struct Mallocator
	{
		typedef T value_type;

		Mallocator() = default;
		template <class U> constexpr Mallocator(const Mallocator <U>&) noexcept {}

		T* allocate(std::size_t n)
		{
#undef max
			if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
				throw std::bad_array_new_length();

			if (auto p = static_cast<T*>(std::malloc(n * sizeof(T)))) {
				return p;
			}

			throw std::bad_alloc();
		}

		void deallocate(T* p, std::size_t n) noexcept {
			std::free(p);
		}
	};

	struct MemoryManagerData
	{
		using StatsMapAlloc = Mallocator<std::pair<const char* const, AllocationStats>>;
		using MapAlloc = Mallocator<std::pair<const void* const, Allocation>>;

		std::map<const char*, AllocationStats, std::less<const char*>, StatsMapAlloc> AllocationStatsMap;
		std::map<const void*, Allocation, std::less<const void*>, MapAlloc> AllocationsMap;
	};

	class MemoryManager
	{
	public:
		static void* Allocate(size_t size);
		static void* Allocate(size_t size, const char* source);
		static void Free(void* memory);
		static size_t GetTotalAllocated();
		static size_t GetTotalFreed();
		static size_t GetCurrentAllocated();
		static std::map<size_t, const char*, std::greater<size_t>> GetCurrentAllocatedSources();

	private:
		static void  Init();

	private:
		inline static MemoryManagerData* s_Data = nullptr;
	};
}

#ifdef ED_TRACK_MEMORY

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size);
void* __CRTDECL operator new(size_t size, const char* source);
_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size);
void* __CRTDECL operator new[](size_t size, const char* source);

void __CRTDECL operator delete(void* memory);
void __CRTDECL operator delete(void* memory, const char* source);
void __CRTDECL operator delete[](void* memory);
void __CRTDECL operator delete[](void* memory, const char* source);

#define enew new("Engine \"enew\" allocation")
#define edelete delete

#else

#define enew new
#define edelete delete

#endif // ED_TRACK_MEMORY