#pragma once

#include <string>
#include <windows.h>
#include <stringapiset.h>

namespace Eden::Utils
{
	inline std::string BytesToString(uint64_t bytes)
	{
		constexpr uint64_t GB = 1024 * 1024 * 1024;
		constexpr uint64_t MB = 1024 * 1024;
		constexpr uint64_t KB = 1024;

		char buffer[32];

		if (bytes > GB)
			sprintf_s(buffer, "%.2f GB", (float)bytes / (float)GB);
		else if (bytes > MB)
			sprintf_s(buffer, "%.2f MB", (float)bytes / (float)MB);
		else if (bytes > KB)
			sprintf_s(buffer, "%.2f KB", (float)bytes / (float)KB);
		else
			sprintf_s(buffer, "%.2f bytes", (float)bytes);

		return std::string(buffer);
	}

	inline void StringConvert(const std::string& from, std::wstring& to)
	{
		int num = MultiByteToWideChar(CP_UTF8, 0, from.c_str(), -1, NULL, 0);
		if (num > 0)
		{
			to.resize(size_t(num) - 1);
			MultiByteToWideChar(CP_UTF8, 0, from.c_str(), -1, &to[0], num);
		}
	}

	inline void StringConvert(const std::wstring& from, std::string& to)
	{
		int num = WideCharToMultiByte(CP_UTF8, 0, from.c_str(), -1, NULL, 0, NULL, NULL);
		if (num > 0)
		{
			to.resize(size_t(num) - 1);
			WideCharToMultiByte(CP_UTF8, 0, from.c_str(), -1, &to[0], num, NULL, NULL);
		}
	}

	inline void StringConvert(const char* from, wchar_t* to)
	{
		int num = MultiByteToWideChar(CP_UTF8, 0, from, -1, NULL, 0);
		if (num > 0)
		{
			MultiByteToWideChar(CP_UTF8, 0, from, -1, &to[0], num);
		}
	}

	inline void StringConvert(const wchar_t* from, char* to)
	{
		int num = WideCharToMultiByte(CP_UTF8, 0, from, -1, NULL, 0, NULL, NULL);
		if (num > 0)
		{
			WideCharToMultiByte(CP_UTF8, 0, from, -1, &to[0], num, NULL, NULL);
		}
	}
}
