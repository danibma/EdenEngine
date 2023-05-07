#pragma once

#include <vector>
#include <string>

namespace Eden
{
	class CommandLine
	{
	public:
		static void Init(const wchar_t* args);
		static void Init(const char* args);
		static bool HasArg(const char* arg);
		static void Parse(const char* arg, std::string& value);
	};
}