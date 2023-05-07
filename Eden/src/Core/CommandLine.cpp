#include "CommandLine.h"
#include "Utilities/Utils.h"
#include "Memory/Memory.h"

namespace Eden
{
	static std::vector<std::string> s_CommandLineArgs;

	void CommandLine::Init(const wchar_t* args)
	{
		const size_t stringSize = wcslen(args);
		char* cmdLineArgs = enew char[stringSize + 1];
		Utils::StringConvert(args, cmdLineArgs);
		Init(cmdLineArgs);
		edelete[] cmdLineArgs;
	}

	void CommandLine::Init(const char* args)
	{
		size_t charNum = strlen(args);
		std::string arg = "";
		for (size_t i = 0; i <= charNum; ++i)
		{
			if (args[i] == '-')
				continue;

			if (args[i] == ' ')
			{
				s_CommandLineArgs.emplace_back(arg);
				arg.clear();
				continue;
			}

			if (i == charNum)
			{
				s_CommandLineArgs.emplace_back(arg);
				continue;
			}

			arg += args[i];
		}
	}

	bool CommandLine::HasArg(const char* arg)
	{
		for (size_t i = 0; i < s_CommandLineArgs.size(); ++i)
		{
			if (s_CommandLineArgs[i] == arg)
				return true;
		}

		return false;
	}

	void CommandLine::Parse(const char* arg, std::string& value)
	{
		value = "";

		size_t argSize = strlen(arg);
		for (size_t i = 0; i < s_CommandLineArgs.size(); ++i)
		{
			std::string_view currentArg = s_CommandLineArgs[i];

			if (currentArg.size() <= argSize)
				continue;

			for (size_t stringIndex = 0; stringIndex < currentArg.size(); ++stringIndex)
			{
				if (stringIndex == argSize && currentArg[stringIndex] == '=')
					value = currentArg.substr(stringIndex + 1);

				if (currentArg[stringIndex] != arg[stringIndex])
					break;
			}
		}
	}

}