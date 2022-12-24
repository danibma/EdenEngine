// Asserts

#include "Log.h"

#if defined(ED_DEBUG) || defined(ED_PROFILING)
#define ED_ASSERT_MB(condition, ...) if(!(condition)) { ED_LOG_FATAL("Assertion Failed: {}", __VA_ARGS__); MessageBoxA(nullptr, #__VA_ARGS__, "Eden Engine Error!", MB_OK); __debugbreak(); }
#define ED_ASSERT_LOG(condition, ...) if(!(condition)) { ED_LOG_FATAL("Assertion Failed: {}", __VA_ARGS__); __debugbreak(); }
#define ED_ASSERT(condition) if(!(condition)) { __debugbreak(); }
#else
#define ED_ASSERT_MB(condition, ...) {}
#define ED_ASSERT_LOG(condition, ...) {}
#define ED_ASSERT(condition) {}
#endif