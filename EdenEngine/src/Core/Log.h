#pragma once

#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

namespace Eden
{
	class Log
	{
	public:
		enum class Level : uint8_t
		{
			Trace = 0, Info, Warn, Error, Fatal
		};

	public:
		static void Init();
		static void Shutdown();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_coreLogger; }

		template<typename... Args>
		static void PrintMessage(Log::Level level, Args&&... args)
		{
			auto logger = GetCoreLogger();
			switch (level)
			{
			case Level::Trace:
				logger->trace(std::forward<Args>(args)...);
				break;
			case Level::Info:
				logger->info(std::forward<Args>(args)...);
				break;
			case Level::Warn:
				logger->warn(std::forward<Args>(args)...);
				break;
			case Level::Error:
				logger->error(std::forward<Args>(args)...);
				break;
			case Level::Fatal:
				logger->critical(std::forward<Args>(args)...);
				break;

			}
		}
	private:
		static std::shared_ptr<spdlog::logger> s_coreLogger;
	};
}

// Use this macros
#define ED_LOG_TRACE(...) ::Eden::Log::PrintMessage(::Eden::Log::Level::Trace, __VA_ARGS__)
#define ED_LOG_INFO(...) ::Eden::Log::PrintMessage(::Eden::Log::Level::Info, __VA_ARGS__)
#define ED_LOG_WARN(...) ::Eden::Log::PrintMessage(::Eden::Log::Level::Warn, __VA_ARGS__)
#define ED_LOG_ERROR(...) ::Eden::Log::PrintMessage(::Eden::Log::Level::Error, __VA_ARGS__)
#define ED_LOG_FATAL(...) ::Eden::Log::PrintMessage(::Eden::Log::Level::Fatal, __VA_ARGS__)
