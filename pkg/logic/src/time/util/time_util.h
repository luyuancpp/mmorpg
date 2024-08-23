#pragma once

#include <chrono>
#include <cstdint>

class TimeUtil
{
public:
	inline static uint64_t NowMilliseconds() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::high_resolution_clock::now().time_since_epoch()
		).count();
	}

	inline static uint64_t NowSeconds() {
		return std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::high_resolution_clock::now().time_since_epoch()
		).count();
	}

};