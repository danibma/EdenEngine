#pragma once

#include <chrono>

namespace Eden
{
	struct Timer
	{
		std::chrono::high_resolution_clock::time_point timestamp;

		// Record a reference timestamp
		inline void Record()
		{
			timestamp = std::chrono::high_resolution_clock::now();
		}

		// Elapsed time in seconds since the Timer creation or last call to record()
		inline double ElapsedSeconds()
		{
			auto timestamp2 = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(timestamp2 - timestamp);
			return time_span.count();
		}

		// Elapsed time in milliseconds since the Timer creation or last call to record()
		inline double ElapsedMilliseconds()
		{
			return ElapsedSeconds() * 1000.0;
		}

		// Elapsed time in milliseconds since the Timer creation or last call to record()
		inline double Elapsed()
		{
			return ElapsedMilliseconds();
		}

	};
}
