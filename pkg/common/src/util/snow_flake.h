#pragma once

#include <chrono>
#include <cstdint>
#include <thread>

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
static constexpr uint64_t kNodeBits = 15;
static constexpr uint64_t kStepBits = 15;

static constexpr uint64_t kTimeShift = kNodeBits + kStepBits;
static constexpr uint64_t kNodeShift = kStepBits;
static constexpr uint64_t kStepMask = (1ULL << kStepBits) - 1;
static constexpr uint64_t kNodeMask = (1ULL << kNodeBits) - 1;

class SnowFlake
{
public:
	SnowFlake() = default;

	inline void set_node_id(uint16_t node_id)
	{
		if (node_id > kNodeMask) {
			LOG_FATAL << "Node ID overflow: max allowed is " << kNodeMask;
		}
		node_id_ = node_id;
	}

	inline uint64_t node_id() const { return node_id_; }

	inline void set_epoch(uint64_t epoch) { epoch_ = epoch; }

	inline Guid Generate()
	{
		uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count()) - epoch_;

		if (now < last_time_) {
			LOG_ERROR << "系统时钟回拨：" << last_time_ << " -> " << now;
			now = WaitNextTime(last_time_);
		}

		if (now > last_time_) {
			step_ = 0;
			last_time_ = now;
		}
		else {
			step_ = (step_ + 1) & kStepMask;
			if (step_ == 0) {
				// 等待下一个秒
				now = WaitNextTime(last_time_);
				last_time_ = now;
			}
		}

		return (last_time_ << kTimeShift) |
			(static_cast<uint64_t>(node_id_) << kNodeShift) |
			step_;
	}

private:
	inline uint64_t WaitNextTime(uint64_t last)
	{
		uint64_t now = NowEpoch();
		while (now <= last) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			now = NowEpoch();
		}
		return now;
	}

	inline uint64_t NowEpoch() const
	{
		return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count()) - epoch_;
	}

	uint64_t epoch_ = kEpoch;
	uint16_t node_id_ = 0;
	uint64_t last_time_ = 0;
	uint64_t step_ = 0;
};

class SnowFlakeAtomic
{
public:
	explicit SnowFlakeAtomic(uint16_t node_id)
		: node_id_(node_id)
	{
		assert(node_id <= kNodeMask);
	}

	Guid Generate()
	{
		while (true) {
			uint64_t now = NowEpoch();

			uint64_t last = last_time_.load(std::memory_order_relaxed);

			if (now < last) {
				LOG_ERROR << "时间回拨：now=" << now << " < last=" << last;
				now = WaitUntilTimeAdvance(last);
			}

			if (now > last) {
				// 抢占新的时间戳
				if (last_time_.compare_exchange_strong(last, now, std::memory_order_acq_rel)) {
					step_.store(0, std::memory_order_relaxed);
					return ComposeID(now, 0);
				}
				// 失败重试（其他线程先抢到了）
				continue;
			}

			// 同一秒内，增加序列号
			uint64_t step = step_.fetch_add(1, std::memory_order_relaxed) & kStepMask;
			if (step == 0) {
				// 本秒 step 用尽，等待下一秒
				now = WaitUntilTimeAdvance(last);
				if (last_time_.compare_exchange_strong(last, now, std::memory_order_acq_rel)) {
					step_.store(0, std::memory_order_relaxed);
					return ComposeID(now, 0);
				}
				continue;
			}

			return ComposeID(now, step);
		}
	}

private:
	inline Guid ComposeID(uint64_t time, uint64_t step)
	{
		return (time << kTimeShift) |
			(static_cast<uint64_t>(node_id_) << kNodeShift) |
			step;
	}

	inline uint64_t NowEpoch() const
	{
		return static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now().time_since_epoch()
			).count()
			) - epoch_;
	}

	inline uint64_t WaitUntilTimeAdvance(uint64_t last_time) const
	{
		uint64_t now = NowEpoch();
		while (now <= last_time) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			now = NowEpoch();
		}
		return now;
	}

private:
	const uint64_t epoch_ = kEpoch;
	const uint16_t node_id_;

	std::atomic<uint64_t> last_time_{ 0 };
	std::atomic<uint64_t> step_{ 0 };
};


struct SnowFlakeComponents {
	uint64_t timestamp;  // 相对于 epoch 的秒数
	uint64_t node_id;
	uint64_t sequence;
};

// 解析 ID 的时间、节点、序列号
inline SnowFlakeComponents ParseGuid(Guid id, uint64_t epoch = kEpoch)
{
	SnowFlakeComponents components;

	components.timestamp = (id >> kTimeShift);
	components.node_id = (id >> kNodeShift) & ((1ULL << kNodeBits) - 1);
	components.sequence = id & kStepMask;

	return components;
}

// 获取真实系统时间戳（std::time_t），单位秒
inline std::time_t GetRealTimeFromGuid(Guid id, uint64_t epoch = kEpoch)
{
	uint64_t seconds_since_epoch = (id >> kTimeShift);
	return static_cast<std::time_t>(seconds_since_epoch + epoch);
}