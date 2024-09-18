#include "time_util.h"

#include <chrono>
#include <cstdint>

uint64_t TimeUtil::NowMilliseconds() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();
}

uint64_t TimeUtil::NowSeconds() {
	return std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();
}

uint64_t TimeUtil::NowMillisecondsUTC() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();
}

uint64_t TimeUtil::NowSecondsUTC() {
	return std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();
}
