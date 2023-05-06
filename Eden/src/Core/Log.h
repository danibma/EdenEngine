#pragma once

#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/base_sink.h>

namespace Eden
{
	template<typename T>
	class OutputLogSynk;

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

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static const std::vector<std::string>& GetOutputLog() { return s_OutputLog; }

		template<typename... Args>
		static void PrintMessage(const Level level, Args&&... args)
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
		template<typename T>
		friend class OutputLogSynk;

		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::vector<std::string> s_OutputLog;
	};

	template<typename Mutex>
	class OutputLogSynk : public spdlog::sinks::base_sink<Mutex>
	{
	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override
		{

			// log_msg is a struct containing the log entry info like level, timestamp, thread id etc.
			// msg.raw contains pre formatted log

			// If needed (very likely but not mandatory), the sink formats the message before sending it to its final destination:
			spdlog::memory_buf_t formatted;
			spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
			Log::s_OutputLog.emplace_back(fmt::to_string(formatted));
		}

		void flush_() override
		{
		}
	};
}

// Use this macros
#define ED_LOG_TRACE(...) ::Eden::Log::PrintMessage(::Eden::Log::Level::Trace, __VA_ARGS__)
#define ED_LOG_INFO(...) ::Eden::Log::PrintMessage(::Eden::Log::Level::Info, __VA_ARGS__)
#define ED_LOG_WARN(...) ::Eden::Log::PrintMessage(::Eden::Log::Level::Warn, __VA_ARGS__)
#define ED_LOG_ERROR(...) ::Eden::Log::PrintMessage(::Eden::Log::Level::Error, __VA_ARGS__)
#define ED_LOG_FATAL(...) ::Eden::Log::PrintMessage(::Eden::Log::Level::Fatal, __VA_ARGS__)
