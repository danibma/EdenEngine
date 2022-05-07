#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

#include <filesystem>

namespace Eden
{
	std::shared_ptr<spdlog::logger> Log::s_coreLogger;

	void Log::Init()
	{
		// Create logs directory if it doesn't exist
		std::string logsDirectory = "logs";
		if (!std::filesystem::exists(logsDirectory))
			std::filesystem::create_directory(logsDirectory);

		std::vector<spdlog::sink_ptr> coreSinks =
		{
			std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/core.log", true),
			std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
			std::make_shared<spdlog::sinks::msvc_sink_mt>()
		};

		const std::string corePattern = "%^%n[%l]: %v%$";
		coreSinks[0]->set_pattern("[%T] %n[%l]: %v");
		coreSinks[1]->set_pattern(corePattern);
		coreSinks[2]->set_pattern(corePattern);

		s_coreLogger = std::make_shared<spdlog::logger>("EDEN", coreSinks.begin(), coreSinks.end());
		s_coreLogger->set_level(spdlog::level::trace);
	}

	void Log::Shutdown()
	{
		s_coreLogger.reset();
		spdlog::drop_all();
	}

}