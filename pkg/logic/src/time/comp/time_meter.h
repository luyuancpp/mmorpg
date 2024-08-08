#pragma once

#include <chrono>

class TimeMeter {
public:
	TimeMeter() : m_start(0), m_duration(0) {}

	// ����ʱ�����������ʼʱ���ͳ���ʱ�䣨����Ϊ��λ��
	void SetStartTime(uint64_t startSeconds) {
		m_start = startSeconds;
	}

	void SetDuration(uint64_t durationSeconds) {
		m_duration = durationSeconds;
	}

	// ����ʣ��ʱ�䣨�룩
	uint64_t Remaining() const {
		auto now = std::chrono::high_resolution_clock::now();
		auto currentSeconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
		if (currentSeconds < m_start) {
			return 0; // �����ǰʱ��С�ڿ�ʼʱ�䣬����ʣ��ʱ��Ϊ0
		}
		auto elapsed = currentSeconds - m_start;
		auto remaining = m_duration > elapsed ? m_duration - elapsed : 0;
		return remaining;
	}

	// ���ؿ�ʼʱ��㣨�룩
	uint64_t Start() const {
		return m_start;
	}

	// ���س���ʱ�䣨�룩
	uint64_t Duration() const {
		return m_duration;
	}

	// ���ʱ��������Ƿ�ʱ
	bool IsExpired() const {
		return Remaining() <= 0;
	}

	// ����ʱ�������
	void Reset() {
		auto now = std::chrono::high_resolution_clock::now();
		m_start = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
	}

private:
	uint64_t m_start;
	uint64_t m_duration;
};


