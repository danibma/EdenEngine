#pragma once

#if defined(ED_PROFILING)
#define PROFILE // For PIX
#endif


#ifndef ED_PLATFORM_WINDOWS
#error Eden only supports Windows!
#endif

// Main macro
#ifdef ED_DEBUG
#define EdenMain() main(int argc, char *argv[])
#else
#define EdenMain() WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
#endif
