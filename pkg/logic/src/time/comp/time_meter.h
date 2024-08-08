#pragma once

#include <chrono>

class TimeMeter {
public:
	TimeMeter() : m_start(0), m_duration(0) {}

	// 设置时间测量器的起始时间点和持续时间（以秒为单位）
	void SetStartTime(uint64_t startSeconds) {
		m_start = startSeconds;
	}

	void SetDuration(uint64_t durationSeconds) {
		m_duration = durationSeconds;
	}

	// 返回剩余时间（秒）
	uint64_t Remaining() const {
		auto now = std::chrono::high_resolution_clock::now();
		auto currentSeconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
		if (currentSeconds < m_start) {
			return 0; // 如果当前时间小于开始时间，返回剩余时间为0
		}
		auto elapsed = currentSeconds - m_start;
		auto remaining = m_duration > elapsed ? m_duration - elapsed : 0;
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

	// 重置时间测量器
	void Reset() {
		auto now = std::chrono::high_resolution_clock::now();
		m_start = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
	}

private:
	uint64_t m_start;
	uint64_t m_duration;
};


