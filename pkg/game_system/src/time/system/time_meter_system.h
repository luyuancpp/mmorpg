#pragma once

#include <chrono>
#include "time_system.h"
#include "logic/component/time_comp.pb.h"

class TimeMeterSecondSystem {
public:
    // 返回剩余时间（秒）
    static uint64_t Remaining(const TimeMeterComp& timeMeterComp) {
        uint64_t currentSeconds = TimeUtil::NowSecondsUTC();
        uint64_t elapsed = (currentSeconds > timeMeterComp.start())
            ? currentSeconds - timeMeterComp.start()
            : 0;
        return (timeMeterComp.duration() > elapsed)
            ? timeMeterComp.duration() - elapsed
            : 0;
    }

    // 检查时间测量器是否超时
    static bool IsExpired(const TimeMeterComp& timeMeterComp) {
        return Remaining(timeMeterComp) == 0;
    }

    // 检查当前时间是否在开始时间之前
    static bool IsBeforeStart(const TimeMeterComp& timeMeterComp) {
        return TimeUtil::NowSecondsUTC() < timeMeterComp.start();
    }

    // 检查当前时间是否未开始（即是否在开始时间之前）
    static bool IsNotStarted(const TimeMeterComp& timeMeterComp) {
        return IsBeforeStart(timeMeterComp);
    }

    // 重置时间测量器
    static void Reset(TimeMeterComp& timeMeterComp) {
        timeMeterComp.set_start(TimeUtil::NowSecondsUTC());
    }


    // 备用时间获取实现
    /*
    inline static uint64_t GetCurrentTimeInSeconds() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
    }
    */
};


class TimeMeterMillisecondUtil {
public:
	// 返回剩余时间（毫秒）
	static uint64_t Remaining(const TimeMeterComp& timeMeterComp) {
		uint64_t currentMilliseconds = GetCurrentTimeInMilliseconds();
		uint64_t elapsed = (currentMilliseconds > timeMeterComp.start())
			? currentMilliseconds - timeMeterComp.start()
			: 0;
		return (timeMeterComp.duration() > elapsed)
			? timeMeterComp.duration() - elapsed
			: 0;
	}

	// 检查时间测量器是否超时
	static bool IsExpired(const TimeMeterComp& timeMeterComp) {
		return Remaining(timeMeterComp) == 0;
	}

	// 检查当前时间是否在开始时间之前
	static bool IsBeforeStart(const TimeMeterComp& timeMeterComp) {
		return GetCurrentTimeInMilliseconds() < timeMeterComp.start();
	}

	// 检查当前时间是否未开始（即是否在开始时间之前）
	static bool IsNotStarted(const TimeMeterComp& timeMeterComp) {
		return IsBeforeStart(timeMeterComp);
	}

	// 重置时间测量器
	static void Reset(TimeMeterComp& timeMeterComp) {
		timeMeterComp.set_start(GetCurrentTimeInMilliseconds());
	}

	inline static uint64_t GetCurrentTimeInMilliseconds() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::high_resolution_clock::now().time_since_epoch()
		).count();
	}
};