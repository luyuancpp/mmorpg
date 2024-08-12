#pragma once

#include <chrono>

#include "logic/component/time_meter_comp.pb.h"

class TimeMeterUtil {
public:
	TimeMeterUtil(){}

	// 设置时间测量器的起始时间点（以秒为单位）
	void SetStartTime(uint64_t startSeconds) {
		timeMeterComp.set_start(startSeconds);
	}

	// 设置持续时间（以秒为单位）
	void SetDuration(uint64_t durationSeconds) {
		timeMeterComp.set_duration(durationSeconds);
	}

	// 返回剩余时间（秒）
	uint64_t Remaining() const {
		uint64_t currentSeconds = GetCurrentTimeInSeconds();
		if (currentSeconds < timeMeterComp.start()) {
			return 0; // 如果当前时间小于开始时间，返回剩余时间为0
		}
		uint64_t elapsed = currentSeconds - timeMeterComp.start();
		uint64_t remaining = timeMeterComp.duration() > elapsed ? timeMeterComp.duration() - elapsed : 0;
		return remaining;
	}

	// 返回开始时间点（秒）
	uint64_t Start() const {
		return timeMeterComp.start();
	}

	// 返回持续时间（秒）
	uint64_t Duration() const {
		return timeMeterComp.duration();
	}

	// 检查时间测量器是否超时
	bool IsExpired() const {
		return Remaining() <= 0;
	}

	// 检查当前时间是否在开始时间之前
	bool IsBeforeStart() const {
		uint64_t currentSeconds = GetCurrentTimeInSeconds();
		return currentSeconds < timeMeterComp.start();
	}

	// 检查当前时间是否未开始（即是否在开始时间之前）
	bool IsNotStarted() const {
		return IsBeforeStart();
	}

	// 重置时间测量器
	void Reset() {
		timeMeterComp.set_start(GetCurrentTimeInSeconds());
	}

private:
	TimeMeterComp timeMeterComp;

	// 获取当前时间的秒数
	uint64_t GetCurrentTimeInSeconds() const {
		return std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::high_resolution_clock::now().time_since_epoch()
		).count();
	}
};
