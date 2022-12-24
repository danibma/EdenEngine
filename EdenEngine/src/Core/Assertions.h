#pragma once

#include "Log.h"

inline void EnsureMsgImpl(bool condition, const char* msg)
{
	if(!(condition)) 
	{ 
		ED_LOG_FATAL("Assertion Failed: {}", msg); 
		__debugbreak(); 
	}
}

inline void EnsureMsgImpl(bool condition, const std::string& msg)
{
	if(!(condition)) 
	{ 
		ED_LOG_FATAL("Assertion Failed: {}", msg); 
		__debugbreak(); 
	}
}

inline void EnsureImpl(bool condition)
{
	if(!(condition)) 
	{ 
		__debugbreak(); 
	}
}

#if defined(ED_DEBUG) || defined(ED_PROFILING)
#define ensureMsg(condition, msg) EnsureMsgImpl(!!(condition), msg)
#define ensure(condition) EnsureImpl(!!(condition))
#else
#define ensureMsg(condition, ...) {}
#define ensure(condition) {}
#endif