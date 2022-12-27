#include "SharedPtr.h"

#include <unordered_set>

namespace Eden::Memory
{
	static std::unordered_set<void*> s_LiveReferences;

	void AddToLiveReferences(void* ref)
	{
		s_LiveReferences.insert(ref);
	}

	void RemoveFromLiveReferences(void* ref)
	{
		s_LiveReferences.erase(ref);
	}
}