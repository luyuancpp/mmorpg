#pragma once

#include <chrono>
#include "time.h"
#include "proto/common/component/time_comp.pb.h"

class TimeMeterSecondSystem {
public:
    static uint64_t Remaining(const TimeMeterComp& timeMeterComp) {
        uint64_t currentSeconds = TimeSystem::NowSecondsUTC();
        uint64_t elapsed = (currentSeconds > timeMeterComp.start())
            ? currentSeconds - timeMeterComp.start()
            : 0;
        return (timeMeterComp.duration() > elapsed)
            ? timeMeterComp.duration() - elapsed
            : 0;
    }

    static bool IsExpired(const TimeMeterComp& timeMeterComp) {
        return Remaining(timeMeterComp) == 0;
    }

    static bool IsBeforeStart(const TimeMeterComp& timeMeterComp) {
        return TimeSystem::NowSecondsUTC() < timeMeterComp.start();
    }

    static bool IsNotStarted(const TimeMeterComp& timeMeterComp) {
        return IsBeforeStart(timeMeterComp);
    }

    static void Reset(TimeMeterComp& timeMeterComp) {
        timeMeterComp.set_start(TimeSystem::NowSecondsUTC());
    }


    // Alternative time implementation
    /*
    inline static uint64_t GetCurrentTimeInSeconds() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
    }
    */
};


class TimeMeterMillisecondSystem {
public:
	static uint64_t Remaining(const TimeMeterComp& timeMeterComp) {
		uint64_t currentMilliseconds = GetCurrentTimeInMilliseconds();
		uint64_t elapsed = (currentMilliseconds > timeMeterComp.start())
			? currentMilliseconds - timeMeterComp.start()
			: 0;
		return (timeMeterComp.duration() > elapsed)
			? timeMeterComp.duration() - elapsed
			: 0;
	}

	static bool IsExpired(const TimeMeterComp& timeMeterComp) {
		return Remaining(timeMeterComp) == 0;
	}

	static bool IsBeforeStart(const TimeMeterComp& timeMeterComp) {
		return GetCurrentTimeInMilliseconds() < timeMeterComp.start();
	}

	static bool IsNotStarted(const TimeMeterComp& timeMeterComp) {
		return IsBeforeStart(timeMeterComp);
	}

	static void Reset(TimeMeterComp& timeMeterComp) {
		timeMeterComp.set_start(GetCurrentTimeInMilliseconds());
	}

	inline static uint64_t GetCurrentTimeInMilliseconds() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::high_resolution_clock::now().time_since_epoch()
		).count();
	}
};