#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <cstdint>
#include <thread>
#include <vector>
#include <cassert>

#include "muduo/base/Logging.h"

#ifndef GUID_TYPE_DEFINED
#define GUID_TYPE_DEFINED
using Guid = uint64_t;
#endif

// ID structure based on Snowflake algorithm
// Reference:
// https://github.com/yitter/IdGenerator
// https://github.com/bwmarrin/snowflake

#ifdef ENABLE_SNOWFLAKE_TESTING
constexpr uint64_t kEpoch = 0;
constexpr uint64_t kNodeBits = 9;
constexpr uint64_t kStepBits = 21;
#else
constexpr uint64_t kEpoch = 1773446400; // 2026-03-14 00:00:00 UTC
constexpr uint64_t kNodeBits = 17;
constexpr uint64_t kStepBits = 15;
#endif

// Layout: [time : 32 bits (~136 years, until 2162)] [node : 17 bits (131072)] [step : 15 bits (32768/sec)]
// Each node type assigns node_id independently (0..262143)
// Different generators per purpose (item/buff/session) avoid cross-type collision
constexpr uint64_t kTimeShift = kNodeBits + kStepBits;
constexpr uint64_t kNodeShift = kStepBits;
constexpr uint64_t kStepMask = (1ULL << kStepBits) - 1;
constexpr uint64_t kNodeMask = (1ULL << kNodeBits) - 1;

class SnowFlake
{
public:
	inline void set_node_id(uint32_t node_id)
	{
		if (node_id > kNodeMask) {
			LOG_FATAL << "Node ID overflow: max allowed is " << kNodeMask;
		}
		node_id_ = node_id;
	}

	inline uint64_t node_id() const { return node_id_; }

	inline void set_epoch(uint64_t epoch) { epoch_ = epoch; }

	// Prevent ID collision with a previous node that held the same node_id.
	// Sets last_time_ so that Generate() skips any second the old node may have used.
	inline void SetGuardTime(uint64_t guardUtcSeconds)
	{
		uint64_t guardEpoch = guardUtcSeconds - epoch_;
		if (guardEpoch > last_time_) {
			last_time_ = guardEpoch;
			step_ = kStepMask; // exhaust this second so Generate() advances to next
		}
	}

	Guid Generate()
	{
		uint64_t now = NowEpoch();

		if (now < last_time_) {
			LOG_ERROR << "System clock moved backwards: " << last_time_ << " -> " << now;
			now = WaitNextTime(last_time_);
		}

		if (now > last_time_) {
			last_time_ = now;
			step_ = 0;
		}
		else {
			if (step_ >= kStepMask) {
				LOG_WARN << "ID pool exhausted in current second, waiting for the next second";
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
				LOG_ERROR << "System clock moved backwards: " << last_time_ << " -> " << now;
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
				LOG_WARN << "ID pool exhausted in current second, waiting for the next second";
				now = WaitNextTime(last_time_);
				last_time_ = now;
				step_ = 0;
			}
		}

		return ids;
	}

#ifdef ENABLE_SNOWFLAKE_TESTING
	void set_mock_now(uint64_t mock_now)
	{
		mock_now_ = mock_now;
		use_mock_time_ = true;
	}
#endif

private:
	uint64_t ComposeID(uint64_t time, uint64_t step)
	{
		return (time << kTimeShift) |
			(static_cast<uint64_t>(node_id_) << kNodeShift) |
			step;
	}

	uint64_t WaitNextTime(uint64_t last)
	{
		uint64_t now = NowEpoch();
		int retry = 0;
		while (now <= last) {
			if (++retry > 3000) {
				LOG_FATAL << "System time not advancing, possible clock anomaly";
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			now = NowEpoch();
		}
		return now;
	}

	uint64_t NowEpoch()
	{
#ifdef ENABLE_SNOWFLAKE_TESTING
		if (use_mock_time_) {
			auto now_epoch = mock_now_++ - epoch_;
			//CheckTimestampOverflow(now_epoch, kTimeShift, epoch_);
			return now_epoch;
		}
#endif

		uint64_t now_epoch = static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now().time_since_epoch())
			.count()) - epoch_;

		//CheckTimestampOverflow(now_epoch, kTimeShift, epoch_);
		return now_epoch;

	}

	inline void CheckTimestampOverflow(uint64_t now_epoch, uint64_t kTimeShift, uint64_t epoch) {
		constexpr uint64_t kSecondsPerYear = 31536000; // 365 * 24 * 60 * 60
		constexpr uint64_t kTimeThreshold = 100000000; // ~3.17 years

		uint64_t max_time_value = (1ULL << (64 - kTimeShift));

		if (now_epoch >= max_time_value) {
			LOG_FATAL << "Snowflake time field overflow! now=" << now_epoch
				<< ", max=" << max_time_value
				<< ", base epoch=" << epoch;
		}
		else if (max_time_value - now_epoch <= kTimeThreshold) {
			LOG_WARN << "Snowflake timestamp approaching limit. Remaining seconds: "
				<< (max_time_value - now_epoch)
				<< " (~" << (max_time_value - now_epoch) / kSecondsPerYear
				<< " years left)";
		}
	}


	inline uint64_t GetMaxSupportedYears(uint64_t kTimeShift) {
		constexpr uint64_t seconds_per_year = 31536000;
		uint64_t time_bits = 64 - kTimeShift;
		uint64_t max_seconds = (1ULL << time_bits);
		return max_seconds / seconds_per_year;
	}

private:
	uint64_t epoch_ = kEpoch;
	uint32_t node_id_ = 0;
	uint64_t last_time_ = 0;
	uint64_t step_ = 0;
#ifdef ENABLE_SNOWFLAKE_TESTING
	uint64_t mock_now_ = 0;
	bool use_mock_time_ = false;
#endif

};

class SnowFlakeAtomic {
public:
	void set_node_id(uint32_t node_id) {
		if (node_id > kNodeMask) {
			LOG_FATAL << "Node ID overflow: max allowed is " << kNodeMask;
		}
		node_id_ = node_id;
	}

	inline void set_epoch(uint64_t epoch) { epoch_ = epoch; }

#ifdef ENABLE_SNOWFLAKE_TESTING
public:
	void set_mock_static_time(uint64_t t) {
		mock_now_.store(t, std::memory_order_relaxed);
		static_mock_mode_.store(true, std::memory_order_relaxed);
		use_mock_time_.store(true, std::memory_order_relaxed);
	}
#endif


	Guid Generate() {
		while (true) {
			uint64_t now = NowEpoch();

			uint64_t current = time_step_.load(std::memory_order_relaxed);
			uint64_t last_time = (current >> kStepBits);
			uint64_t last_step = (current & kStepMask);

			if (now < last_time) {
				LOG_ERROR << "Clock rollback: now=" << now << " < last=" << last_time;
				now = WaitUntilTimeAdvance(last_time);
				continue;
			}

			uint64_t next;
			uint64_t step_to_use;

			if (now == last_time) {
				if (last_step >= kStepMask) {
					now = WaitUntilTimeAdvance(last_time);
					continue;
				}
				step_to_use = last_step + 1;
				next = (now << kStepBits) | step_to_use;
			}
			else {
				step_to_use = 0;
				next = (now << kStepBits);
			}

			if (time_step_.compare_exchange_weak(current, next, std::memory_order_acq_rel)) {
				return ComposeID(now, step_to_use);
			}
		}
	}

	std::vector<Guid> GenerateBatch(size_t count) {
		std::vector<Guid> ids;
		ids.reserve(count);

		while (count > 0) {
			uint64_t now = NowEpoch();

			uint64_t current = time_step_.load(std::memory_order_relaxed);
			uint64_t last_time = (current >> kStepBits);
			uint64_t last_step = (current & kStepMask);

			if (now < last_time) {
				LOG_ERROR << "Clock rollback: now=" << now << " < last=" << last_time;
				now = WaitUntilTimeAdvance(last_time);
				continue;
			}

			uint64_t next;
			uint64_t step_start = 0;
			uint64_t step_count = 0;

			if (now == last_time) {
				if (last_step >= kStepMask) {
					now = WaitUntilTimeAdvance(last_time);
					continue;
				}
				step_start = last_step + 1;
				step_count = std::min(static_cast<size_t>(kStepMask - last_step), count);
				next = (now << kStepBits) | (step_start + step_count - 1);
			}
			else {
				step_start = 0;
				step_count = std::min(static_cast<size_t>(kStepMask + 1), count);
				next = (now << kStepBits) | (step_count - 1);
			}

			if (time_step_.compare_exchange_weak(current, next, std::memory_order_acq_rel)) {
				for (uint64_t i = 0; i < step_count; ++i) {
					ids.push_back(ComposeID(now, step_start + i));
				}
				count -= step_count;
			}
		}

		return ids;
	}

private:
	Guid ComposeID(uint64_t time, uint64_t step) {
		return (time << kTimeShift) |
			(static_cast<uint64_t>(node_id_) << kNodeShift) |
			step;
	}

	uint64_t NowEpoch() {
#ifdef ENABLE_SNOWFLAKE_TESTING
		if (use_mock_time_.load(std::memory_order_relaxed)) {
			if (static_mock_mode_.load(std::memory_order_relaxed)) {
				return mock_now_.load(std::memory_order_relaxed) - epoch_;
			}
			return mock_now_.fetch_add(1, std::memory_order_relaxed) - epoch_;
		}
#endif
		return static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now().time_since_epoch())
			.count()) -
			epoch_;
	}


	uint64_t WaitUntilTimeAdvance(uint64_t last_time) {
		uint64_t now = NowEpoch();
		int retry = 0;
		while (now <= last_time) {
			if (++retry > 3000) {
				LOG_FATAL << "System time not advancing";
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			now = NowEpoch();
		}
		return now;
	}

private:
	uint64_t epoch_ = kEpoch;

	uint32_t node_id_{ 0 };
	std::atomic<uint64_t> time_step_{ 0 };

#ifdef ENABLE_SNOWFLAKE_TESTING
	std::atomic<uint64_t> mock_now_{ 0 };
	std::atomic<bool> use_mock_time_{ false };
	std::atomic<bool> static_mock_mode_{ false };
#endif

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
