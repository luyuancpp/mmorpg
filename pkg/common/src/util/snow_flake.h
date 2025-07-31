#pragma once

#include <chrono>
#include <cstdint>
#include <thread>
#include <atomic>
#include <cassert>

#include "muduo/base/Logging.h"

#ifndef GUID_TYPE_DEFINED
#define GUID_TYPE_DEFINED
using Guid = uint64_t;
#endif

// ID 结构参考自 Snowflake 算法
// 链接参考：
// https://github.com/yitter/IdGenerator
// https://github.com/bwmarrin/snowflake

static constexpr uint64_t kEpoch = 1719674201;
static constexpr uint64_t kNodeBits = 12;
static constexpr uint64_t kStepBits = 18;

static constexpr uint64_t kTimeShift = kNodeBits + kStepBits;
static constexpr uint64_t kNodeShift = kStepBits;
static constexpr uint64_t kStepMask = (1ULL << kStepBits) - 1;
static constexpr uint64_t kNodeMask = (1ULL << kNodeBits) - 1;

class SnowFlake
{
public:
	inline void set_node_id(uint16_t node_id)
	{
		if (node_id > kNodeMask) {
			LOG_FATAL << "Node ID overflow: max allowed is " << kNodeMask;
		}
		node_id_ = node_id;
	}

	inline uint64_t node_id() const { return node_id_; }

	inline void set_epoch(uint64_t epoch) { epoch_ = epoch; }

	Guid Generate()
	{
		uint64_t now = NowEpoch();

		if (now < last_time_) {
			LOG_ERROR << "系统时钟回拨：" << last_time_ << " -> " << now;
			now = WaitNextTime(last_time_);
		}

		if (now > last_time_) {
			last_time_ = now;
			step_ = 0;
		}
		else {
			if (step_ >= kStepMask) {
				LOG_WARN << "当前秒内 ID 已耗尽，等待下一秒";
				now = WaitNextTime(last_time_);
				last_time_ = now;
				step_ = 0;
			}
			else {
				step_ += 1;
			}
		}

		return ComposeID(last_time_, step_);
	}


	std::vector<Guid> GenerateBatch(size_t count)
	{
		std::vector<Guid> ids;
		ids.reserve(count);

		while (count > 0) {
			uint64_t now = NowEpoch();

			if (now < last_time_) {
				LOG_ERROR << "系统时钟回拨：" << last_time_ << " -> " << now;
				now = WaitNextTime(last_time_);
			}

			if (now > last_time_) {
				step_ = 0;
				last_time_ = now;
			}

			uint64_t remaining = kStepMask - step_ + 1;
			uint64_t batch = std::min<uint64_t>(remaining, count);

			for (uint64_t i = 0; i < batch; ++i) {
				ids.push_back(ComposeID(now, step_++));
			}

			count -= batch;

			if (step_ > kStepMask) {
				LOG_WARN << "当前秒内 ID 已耗尽，等待下一秒";
				now = WaitNextTime(last_time_);
				last_time_ = now;
				step_ = 0;
			}
		}

		return ids;
	}

private:
	uint64_t ComposeID(uint64_t time, uint64_t step)
	{
		return (time << kTimeShift) |
			(static_cast<uint64_t>(node_id_) << kNodeShift) |
			step;
	}

	uint64_t WaitNextTime(uint64_t last) const
	{
		uint64_t now = NowEpoch();
		int retry = 0;
		while (now <= last) {
			if (++retry > 3000) {  // 最多等待约3秒
				LOG_FATAL << "系统时间未前进，可能时钟异常";
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			now = NowEpoch();
		}
		return now;
	}

	uint64_t NowEpoch() const
	{
		return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count()) -
			epoch_;
	}

private:
	uint64_t epoch_ = kEpoch;
	uint16_t node_id_;
	uint64_t last_time_ = 0;
	uint64_t step_ = 0;
};

class SnowFlakeAtomic
{
public:
	inline void set_node_id(uint16_t node_id)
	{
		if (node_id > kNodeMask) {
			LOG_FATAL << "Node ID overflow: max allowed is " << kNodeMask;
		}
		node_id_ = node_id;
	}

	Guid Generate()
	{
		while (true) {
			uint64_t now = NowEpoch();
			uint64_t last = last_time_.load(std::memory_order_relaxed);

			if (now < last) {
				LOG_ERROR << "时间回拨：now=" << now << " < last=" << last;
				now = WaitUntilTimeAdvance(last);
				last = last_time_.load(std::memory_order_relaxed); // reload
			}

			if (now > last) {
				if (last_time_.compare_exchange_strong(last, now, std::memory_order_acq_rel)) {
					step_.store(0, std::memory_order_relaxed);
					last = now;
				}
				else {
					last = last_time_.load(std::memory_order_relaxed);
					continue;
				}
			}

			uint64_t step = step_.fetch_add(1, std::memory_order_relaxed);
			if (step > kStepMask) {
				LOG_WARN << "step overflow in one second, waiting for next second";
				now = WaitUntilTimeAdvance(last);
				last_time_.store(now, std::memory_order_relaxed);
				step_.store(0, std::memory_order_relaxed);
				continue;
			}
			return ComposeID(last, step);
		}
	}


	std::vector<Guid> GenerateBatch(size_t count)
	{
		std::vector<Guid> ids;
		ids.reserve(count);

		while (count > 0) {
			uint64_t now = NowEpoch();
			uint64_t last = last_time_.load(std::memory_order_relaxed);

			if (now < last) {
				LOG_ERROR << "时间回拨：now=" << now << " < last=" << last;
				now = WaitUntilTimeAdvance(last);
				last = last_time_.load(std::memory_order_relaxed);
			}

			if (now > last) {
				if (last_time_.compare_exchange_strong(last, now, std::memory_order_acq_rel)) {
					step_.store(0, std::memory_order_relaxed);
					last = now;
				}
				else {
					continue; // Retry
				}
			}

			uint64_t current_step = step_.fetch_add(count, std::memory_order_relaxed);
			uint64_t available = kStepMask - current_step + 1;

			uint64_t batch = std::min<uint64_t>(available, count);

			for (uint64_t i = 0; i < batch; ++i) {
				ids.push_back(ComposeID(now, current_step + i));
			}

			count -= batch;

			if (current_step + batch - 1 >= kStepMask) {
				LOG_WARN << "当前秒内 ID 已耗尽（并发），等待下一秒";
				now = WaitUntilTimeAdvance(last);
				last_time_.store(now, std::memory_order_relaxed);
				step_.store(0, std::memory_order_relaxed);
			}
		}

		return ids;
	}


private:
	Guid ComposeID(uint64_t time, uint64_t step)
	{
		return (time << kTimeShift) |
			(static_cast<uint64_t>(node_id_) << kNodeShift) |
			step;
	}

	uint64_t NowEpoch() const
	{
		return static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now().time_since_epoch())
			.count()) -
			epoch_;
	}

	uint64_t WaitUntilTimeAdvance(uint64_t last_time) const
	{
		uint64_t now = NowEpoch();
		int retry = 0;
		while (now <= last_time) {
			if (++retry > 3000) {
				LOG_FATAL << "系统时间未前进，可能时钟异常";
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			now = NowEpoch();
		}
		return now;
	}

private:
	const uint64_t epoch_ = kEpoch;
	uint16_t node_id_;
	std::atomic<uint64_t> last_time_{ 0 };
	std::atomic<uint64_t> step_{ 0 };
};

struct SnowFlakeComponents {
	uint64_t timestamp;
	uint64_t node_id;
	uint64_t sequence;
};

inline SnowFlakeComponents ParseGuid(Guid id, uint64_t epoch = kEpoch)
{
	SnowFlakeComponents components;
	components.timestamp = (id >> kTimeShift);
	components.node_id = (id >> kNodeShift) & kNodeMask;
	components.sequence = id & kStepMask;
	return components;
}

inline std::time_t GetRealTimeFromGuid(Guid id, uint64_t epoch = kEpoch)
{
	uint64_t seconds_since_epoch = (id >> kTimeShift);
	return static_cast<std::time_t>(seconds_since_epoch + epoch);
}
