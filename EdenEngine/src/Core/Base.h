#pragma once

#include "Log.h"


#ifndef ED_PLATFORM_WINDOWS
#error Eden only supports Windows!
#endif

// Asserts
#ifdef ED_DEBUG
#define ED_ASSERT_MB(condition, ...) if(!(condition)) { ED_LOG_FATAL("Assertion Failed: {}", __VA_ARGS__); MessageBoxA(nullptr, #__VA_ARGS__, "Eden Engine Error!", MB_OK); __debugbreak(); }
#define ED_ASSERT_LOG(condition, ...) if(!(condition)) { ED_LOG_FATAL("Assertion Failed: {}", __VA_ARGS__); __debugbreak(); }
#define ED_ASSERT(condition) if(!(condition)) { __debugbreak(); }
#else
#define ED_ASSERT_MB(condition, ...) {}
#define ED_ASSERT_LOG(condition, ...) {}
#define ED_ASSERT(condition) {}
#endif

// Main macro
#ifdef ED_DEBUG
#define EdenMain() main(int argc, char *argv[])
#else
#define EdenMain() WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
#endif

// Graphics macros
#define SAFE_RELEASE(x) if (x) x->Release();
