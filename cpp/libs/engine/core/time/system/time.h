#pragma once

#include <cstdint>

class TimeSystem
{
public:
	static uint64_t NowMicroseconds();

	static uint64_t NowMilliseconds();

	static uint64_t NowSeconds();

	static uint64_t NowMillisecondsUTC();

	static uint64_t NowMicrosecondsUTC();

	static uint64_t NowSecondsUTC();
};