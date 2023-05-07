#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

#include <filesystem>
#include <iostream>

namespace Eden
{
	std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
	std::vector<std::string> Log::s_OutputLog;

	void Log::Init()
	{
		std::vector<spdlog::sink_ptr> coreSinks =
		{
			std::make_shared<spdlog::sinks::basic_file_sink_mt>("log.txt", false),
			std::make_shared<spdlog::sinks::msvc_sink_mt>(),
			std::make_shared<OutputLogSynk<std::mutex>>()
		};

		const std::string corePattern = "%^%n[%l]: %v%$";
		coreSinks[0]->set_pattern("[%T] %n[%l]: %v");
		coreSinks[1]->set_pattern(corePattern);
		coreSinks[2]->set_pattern(corePattern);

		s_CoreLogger = std::make_shared<spdlog::logger>("EDEN", coreSinks.begin(), coreSinks.end());
		s_CoreLogger->set_level(spdlog::level::trace);

		ED_LOG_INFO("Log manager has been initialized!");
	}

	void Log::Shutdown()
	{
		s_CoreLogger.reset();
		spdlog::drop_all();
	}

}