#pragma once
#include <chrono>

class TimeMeter {
public:
	TimeMeter() : m_start(std::chrono::high_resolution_clock::now()), m_duration(std::chrono::seconds(0)) {}

	// 设置时间测量器的持续时间
	void SetDuration(std::chrono::seconds duration) {
		m_duration = duration;
	}

	// 返回剩余时间（秒）
	uint64_t Remaining() const {
		auto now = std::chrono::high_resolution_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_start);
		auto remaining = m_duration - elapsed;
		return remaining.count();
	}

	// 检查时间测量器是否超时
	bool IsExpired() const {
		return Remaining() <= 0;
	}

	// 重置时间测量器
	void Reset() {
		m_start = std::chrono::high_resolution_clock::now();
	}

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
	std::chrono::seconds m_duration;
};

