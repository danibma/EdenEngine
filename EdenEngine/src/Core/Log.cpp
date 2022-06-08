#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

#include <filesystem>

namespace Eden
{
	std::shared_ptr<spdlog::logger> Log::s_CoreLogger;

	void Log::Init()
	{
		// Create logs directory if it doesn't exist
		std::string logs_directory = "logs";
		if (!std::filesystem::exists(logs_directory))
			std::filesystem::create_directory(logs_directory);

		std::vector<spdlog::sink_ptr> core_sinks =
		{
			std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/core.log", true),
			std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
			std::make_shared<spdlog::sinks::msvc_sink_mt>()
		};

		const std::string core_pattern = "%^%n[%l]: %v%$";
		core_sinks[0]->set_pattern("[%T] %n[%l]: %v");
		core_sinks[1]->set_pattern(core_pattern);
		core_sinks[2]->set_pattern(core_pattern);

		s_CoreLogger = std::make_shared<spdlog::logger>("EDEN", core_sinks.begin(), core_sinks.end());
		s_CoreLogger->set_level(spdlog::level::trace);
	}

	void Log::Shutdown()
	{
		s_CoreLogger.reset();
		spdlog::drop_all();
	}

}