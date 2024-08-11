#pragma once

#include <chrono>

class TimeMeter {
public:
	TimeMeter() 
		: m_start(0), m_duration(0) {}

	// 设置时间测量器的起始时间点（以秒为单位）
	void SetStartTime(uint64_t startSeconds) {
		m_start = startSeconds;
	}

	// 设置持续时间（以秒为单位）
	void SetDuration(uint64_t durationSeconds) {
		m_duration = durationSeconds;
	}

	// 返回剩余时间（秒）
	uint64_t Remaining() const {
		uint64_t currentSeconds = GetCurrentTimeInSeconds();
		if (currentSeconds < m_start) {
			return 0; // 如果当前时间小于开始时间，返回剩余时间为0
		}
		uint64_t elapsed = currentSeconds - m_start;
		uint64_t remaining = m_duration > elapsed ? m_duration - elapsed : 0;
		return remaining;
	}

	// 返回开始时间点（秒）
	uint64_t Start() const {
		return m_start;
	}

	// 返回持续时间（秒）
	uint64_t Duration() const {
		return m_duration;
	}

	// 检查时间测量器是否超时
	bool IsExpired() const {
		return Remaining() <= 0;
	}

	// 检查当前时间是否在开始时间之前
	bool IsBeforeStart() const {
		uint64_t currentSeconds = GetCurrentTimeInSeconds();
		return currentSeconds < m_start;
	}

	// 检查当前时间是否未开始（即是否在开始时间之前）
	bool IsNotStarted() const {
		return IsBeforeStart();
	}

	// 重置时间测量器
	void Reset() {
		m_start = GetCurrentTimeInSeconds();
	}

private:
	uint64_t m_start;     // 以秒为单位的起始时间点
	uint64_t m_duration;  // 以秒为单位的持续时间

	// 获取当前时间的秒数
	uint64_t GetCurrentTimeInSeconds() const {
		return std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::high_resolution_clock::now().time_since_epoch()
		).count();
	}
};
