#pragma once

#include "Log.h"

#ifndef ED_PLATFORM_WINDOWS
#error Eden only supports Windows!
#endif

#ifndef ED_DIST
#define ED_ASSERT_LOG(condition, ...) if(!(condition)) { ED_LOG_FATAL("Assertion Failed: {}", __VA_ARGS__); __debugbreak(); }
#define ED_ASSERT(condition) if(!(condition)) { __debugbreak(); }
#else
#define ED_ASSERT_LOG(condition, ...)
#define ED_ASSERT(condition)
#endif

