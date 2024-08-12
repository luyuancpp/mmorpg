#pragma once

#include <chrono>

#include "logic/component/time_meter_comp.pb.h"

class TimeMeterUtil {
public:

	// 返回剩余时间（秒）
	static uint64_t Remaining(const TimeMeterComp& timeMeterComp){
		uint64_t currentSeconds = GetCurrentTimeInSeconds();
		if (currentSeconds < timeMeterComp.start()) {
			return 0; // 如果当前时间小于开始时间，返回剩余时间为0
		}
		uint64_t elapsed = currentSeconds - timeMeterComp.start();
		uint64_t remaining = timeMeterComp.duration() > elapsed ? timeMeterComp.duration() - elapsed : 0;
		return remaining;
	}

	// 检查时间测量器是否超时
	static bool IsExpired(const TimeMeterComp& timeMeterComp){
		return Remaining(timeMeterComp) <= 0;
	}

	// 检查当前时间是否在开始时间之前
	static bool IsBeforeStart(const TimeMeterComp& timeMeterComp){
		uint64_t currentSeconds = GetCurrentTimeInSeconds();
		return currentSeconds < timeMeterComp.start();
	}

	// 检查当前时间是否未开始（即是否在开始时间之前）
	static bool IsNotStarted(const TimeMeterComp& timeMeterComp){
		return IsBeforeStart(timeMeterComp);
	}

	// 重置时间测量器
	static void Reset(TimeMeterComp& timeMeterComp) {
		timeMeterComp.set_start(GetCurrentTimeInSeconds());
	}

	static uint64_t GetCurrentTimeInSeconds() {
		return std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::high_resolution_clock::now().time_since_epoch()
		).count();
	}
};
