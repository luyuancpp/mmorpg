#pragma once

#include <chrono>
#include "table/cooldown_config.h"
#include "time_system.h"
#include "proto/logic/component/time_comp.pb.h"

class CoolDownTimeMillisecondSystem {
public:
	// 返回剩余时间（毫秒）
	inline static  uint64_t Remaining(const CooldownTimeComp& cooldownTimeComp) {
		uint64_t currentMilliseconds = NowMilliseconds();
		uint64_t elapsed = (currentMilliseconds > cooldownTimeComp.start())
			? currentMilliseconds - cooldownTimeComp.start()
			: 0;

		// 获取冷却表并处理其为 nullptr 的情况
		FetchCooldownTableOrReturnCustom(cooldownTimeComp.cooldown_table_id(), 0);

		return (cooldownTable->duration() > elapsed)
			? cooldownTable->duration() - elapsed
			: 0;
	}

	// 检查冷却时间是否超时
	inline static  bool IsExpired(const CooldownTimeComp& cooldownTimeComp) {
		return Remaining(cooldownTimeComp) == 0;
	}

	// 检查当前时间是否在开始时间之前
	inline static  bool IsBeforeStart(const CooldownTimeComp& cooldownTimeComp) {
		return NowMilliseconds() < cooldownTimeComp.start();
	}

	// 检查冷却时间是否未开始（即是否在开始时间之前）
	inline static  bool IsNotStarted(const CooldownTimeComp& cooldownTimeComp) {
		return IsBeforeStart(cooldownTimeComp);
	}

	// 重置冷却时间
	inline static  void Reset(CooldownTimeComp& cooldownTimeComp) {
		cooldownTimeComp.set_start(NowMilliseconds());
	}

	// 获取冷却时间的持续时间（毫秒）
	inline static  uint64_t GetDuration(const CooldownTimeComp& cooldownTimeComp) {
		FetchCooldownTableOrReturnCustom(cooldownTimeComp.cooldown_table_id(), 0);
	
		return cooldownTable->duration();
	}

	// 设置冷却时间的开始时间（毫秒）
	inline static  void SetStartTime(CooldownTimeComp& cooldownTimeComp, uint64_t startTimeMilliseconds) {
		cooldownTimeComp.set_start(startTimeMilliseconds);
	}

	// 获取冷却时间的开始时间（毫秒）
	inline static  uint64_t GetStartTime(const CooldownTimeComp& cooldownTimeComp) {
		return cooldownTimeComp.start();
	}

	// 获取当前时间（以毫秒为单位）
	inline static uint64_t NowMilliseconds() {
		return TimeUtil::NowMilliseconds();
	}

	inline static bool IsCooldownComplete(const CooldownTimeComp& cooldownTimeComp) {
		return Remaining(cooldownTimeComp) == 0;
	}

	inline static bool IsInCooldown(const CooldownTimeComp& cooldownTimeComp) {
		return GetStartTime(cooldownTimeComp) != 0 && !IsCooldownComplete(cooldownTimeComp);
	}

	inline static void ResetCooldown(CooldownTimeComp& cooldownTimeComp) {
		SetStartTime(cooldownTimeComp, NowMilliseconds());
	}
};


