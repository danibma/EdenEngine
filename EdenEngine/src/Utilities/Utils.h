#pragma once

#include <string>

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
}
