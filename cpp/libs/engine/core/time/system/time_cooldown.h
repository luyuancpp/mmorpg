#pragma once

#include <chrono>
#include "table/code/cooldown_table.h"
#include "time.h"
#include "proto/common/component/time_comp.pb.h"

class CoolDownTimeMillisecondSystem {
public:
	inline static  uint64_t Remaining(const CooldownTimeComp& cooldownTimeComp) {
		uint64_t currentMilliseconds = NowMilliseconds();
		uint64_t elapsed = (currentMilliseconds > cooldownTimeComp.start())
			? currentMilliseconds - cooldownTimeComp.start()
			: 0;

		LookupCooldownOrReturn(cooldownTimeComp.cooldown_table_id(), 0);

		return (cooldownRow->duration() > elapsed)
			? cooldownRow->duration() - elapsed
			: 0;
	}

	inline static  bool IsExpired(const CooldownTimeComp& cooldownTimeComp) {
		return Remaining(cooldownTimeComp) == 0;
	}

	inline static  bool IsBeforeStart(const CooldownTimeComp& cooldownTimeComp) {
		return NowMilliseconds() < cooldownTimeComp.start();
	}

	inline static  bool IsNotStarted(const CooldownTimeComp& cooldownTimeComp) {
		return IsBeforeStart(cooldownTimeComp);
	}

	inline static  void Reset(CooldownTimeComp& cooldownTimeComp) {
		cooldownTimeComp.set_start(NowMilliseconds());
	}

	inline static  uint64_t GetDuration(const CooldownTimeComp& cooldownTimeComp) {
		LookupCooldownOrReturn(cooldownTimeComp.cooldown_table_id(), 0);
	
		return cooldownRow->duration();
	}

	inline static  void SetStartTime(CooldownTimeComp& cooldownTimeComp, uint64_t startTimeMilliseconds) {
		cooldownTimeComp.set_start(startTimeMilliseconds);
	}

	inline static  uint64_t GetStartTime(const CooldownTimeComp& cooldownTimeComp) {
		return cooldownTimeComp.start();
	}

	inline static uint64_t NowMilliseconds() {
		return TimeSystem::NowMilliseconds();
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


