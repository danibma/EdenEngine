#pragma once

#include "Log.h"

#if defined(ED_DEBUG) || defined(ED_PROFILING)
#ifdef ED_PLATFORM_WINDOWS
#define PLATFORM_BREAK() __debugbreak()
#else
#define PLATFORM_BREAK()
#endif // ED_PLATFORM_WINDOWS
#else
#define PLATFORM_BREAK()
#endif // defined(ED_DEBUG) || defined(ED_PROFILING)

template<typename... Args>
inline void EnsureMsgfImpl(bool condition, const char* format, Args&&... args)
{
	if(!(condition)) 
	{ 
		const size_t size = 1024;
		char buf[size];
		snprintf(buf, size, format, args...);
		ED_LOG_FATAL("Assertion failed: {}", buf); 
		PLATFORM_BREAK();
	}
}

inline void EnsureMsgImpl(bool condition, const char* msg)
{
	if(!(condition)) 
	{ 
		ED_LOG_FATAL("Assertion failed: {}", msg); 
		PLATFORM_BREAK();
	}
}

inline void EnsureMsgImpl(bool condition, const std::string& msg)
{
	if(!(condition)) 
	{ 
		ED_LOG_FATAL("Assertion failed: {}", msg); 
		PLATFORM_BREAK();
	}
}

inline void EnsureImpl(bool condition, const char* file, int line, const char* function)
{
	if(!(condition)) 
	{ 
		ED_LOG_FATAL("Assertion \"{}\" failed on {}:{} - {}", condition, file, line, function);
		PLATFORM_BREAK();
	}
}

#define ensureMsgf(condition, format, ...) EnsureMsgfImpl(!!(condition), format, __VA_ARGS__)
#define ensureMsg(condition, msg) EnsureMsgImpl(!!(condition), msg)
#define ensure(condition) EnsureImpl(!!(condition), __FILE__, __LINE__, __func__)