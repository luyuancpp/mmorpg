#pragma once

#include <chrono>
#include <muduo/base/Timestamp.h>
#include "logic/component/time_comp.pb.h"
#include "cooldown_config.h"

class CoolDownTimeMillisecondUtil {
public:
	// ����ʣ��ʱ�䣨���룩
	inline static  uint64_t Remaining(const CooldownTimeComp& cooldownTimeComp) {
		uint64_t currentMilliseconds = GetCurrentTimeInMilliseconds();
		uint64_t elapsed = (currentMilliseconds > cooldownTimeComp.start())
			? currentMilliseconds - cooldownTimeComp.start()
			: 0;

		// ��ȡ��ȴ��������Ϊ nullptr �����
		auto table = GetCooldownTable(cooldownTimeComp.cooldown_table_id());
		if (table == nullptr) {
			// ��¼��־����������ʽ
			return 0;
		}

		return (table->duration() > elapsed)
			? table->duration() - elapsed
			: 0;
	}

	// �����ȴʱ���Ƿ�ʱ
	inline static  bool IsExpired(const CooldownTimeComp& cooldownTimeComp) {
		return Remaining(cooldownTimeComp) == 0;
	}

	// ��鵱ǰʱ���Ƿ��ڿ�ʼʱ��֮ǰ
	inline static  bool IsBeforeStart(const CooldownTimeComp& cooldownTimeComp) {
		return GetCurrentTimeInMilliseconds() < cooldownTimeComp.start();
	}

	// �����ȴʱ���Ƿ�δ��ʼ�����Ƿ��ڿ�ʼʱ��֮ǰ��
	inline static  bool IsNotStarted(const CooldownTimeComp& cooldownTimeComp) {
		return IsBeforeStart(cooldownTimeComp);
	}

	// ������ȴʱ��
	inline static  void Reset(CooldownTimeComp& cooldownTimeComp) {
		cooldownTimeComp.set_start(GetCurrentTimeInMilliseconds());
	}

	// ��ȡ��ȴʱ��ĳ���ʱ�䣨���룩
	inline static  uint64_t GetDuration(const CooldownTimeComp& cooldownTimeComp) {

		auto table = GetCooldownTable(cooldownTimeComp.cooldown_table_id());
		if (table == nullptr) {
			// ��¼��־����������ʽ
			return 0;
		}

		return table->duration();
	}

	// ������ȴʱ��Ŀ�ʼʱ�䣨���룩
	inline static  void SetStartTime(CooldownTimeComp& cooldownTimeComp, uint64_t startTimeMilliseconds) {
		cooldownTimeComp.set_start(startTimeMilliseconds);
	}

	// ��ȡ��ȴʱ��Ŀ�ʼʱ�䣨���룩
	inline static  uint64_t GetStartTime(const CooldownTimeComp& cooldownTimeComp) {
		return cooldownTimeComp.start();
	}

	// ��ȡ��ǰʱ�䣨�Ժ���Ϊ��λ��
	inline static uint64_t GetCurrentTimeInMilliseconds() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::high_resolution_clock::now().time_since_epoch()
		).count();
	}

	inline static bool IsCooldownComplete(const CooldownTimeComp& cooldownTimeComp) {
		return Remaining(cooldownTimeComp) == 0;
	}

	inline static bool IsInCooldown(const CooldownTimeComp& cooldownTimeComp) {
		return GetStartTime(cooldownTimeComp) != 0 && !IsCooldownComplete(cooldownTimeComp);
	}

	inline static void ResetCooldown(CooldownTimeComp& cooldownTimeComp) {
		SetStartTime(cooldownTimeComp, GetCurrentTimeInMilliseconds());
	}
};


