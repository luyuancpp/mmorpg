#pragma once
#include <chrono>

class TimeMeter {
public:
	TimeMeter() : m_start(std::chrono::high_resolution_clock::now()), m_duration(std::chrono::seconds(0)) {}

	// ����ʱ��������ĳ���ʱ��
	void SetDuration(std::chrono::seconds duration) {
		m_duration = duration;
	}

	// ����ʣ��ʱ�䣨�룩
	uint64_t Remaining() const {
		auto now = std::chrono::high_resolution_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_start);
		auto remaining = m_duration - elapsed;
		return remaining.count();
	}

	// ���ʱ��������Ƿ�ʱ
	bool IsExpired() const {
		return Remaining() <= 0;
	}

	// ����ʱ�������
	void Reset() {
		m_start = std::chrono::high_resolution_clock::now();
	}

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
	std::chrono::seconds m_duration;
};

