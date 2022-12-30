#pragma once

#include <vector>

namespace Eden
{
	class CommandLine
	{
	public:
		static void Init(const wchar_t* args);
		static void Init(const char* args);
		static bool HasArg(const char* arg);
	};
}