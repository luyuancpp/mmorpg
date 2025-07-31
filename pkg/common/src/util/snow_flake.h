#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>

#include "muduo/base/Logging.h"
#include "type_define/type_define.h"  // 假设这里包含了你定义 Guid 的地方

// 定义 Guid 类型，如果没定义
#ifndef GUID_TYPE_DEFINED
#define GUID_TYPE_DEFINED
using Guid = uint64_t;
#endif

// ID 结构参考自 Snowflake 算法
// 链接参考：
// https://github.com/yitter/IdGenerator
// https://github.com/bwmarrin/snowflake

// 注意：总位数为 63 位，兼容 Lua（避免使用符号位）

static constexpr uint64_t kEpoch = 1719674201; // 自定义 epoch（单位：秒）
static constexpr uint64_t kNodeBits = 13;      // 节点ID位数
static constexpr uint64_t kStepBits = 18;      // 每秒内生成的序号
static constexpr uint64_t kTimeShift = kNodeBits + kStepBits;
static constexpr uint64_t kNodeShift = kStepBits;
static constexpr uint64_t kStepMask = (1ULL << kStepBits) - 1; // 262143

class SnowFlake
{
public:
	SnowFlake(const SnowFlake&) = delete;
	SnowFlake& operator=(const SnowFlake&) = delete;

	SnowFlake() {}

	inline void set_node_id(uint16_t node_id)
	{
		if (node_id >= (1 << kNodeBits)) {
			LOG_FATAL << "Node ID overflow: max allowed is " << ((1 << kNodeBits) - 1);
		}
		node_id_ = node_id;
		node_ = static_cast<uint64_t>(node_id_) & ((1ULL << kNodeBits) - 1);  // 正确掩码
	}

	inline uint64_t node_id() const { return node_id_; }

	inline void set_epoch(uint64_t epoch) { epoch_ = epoch; }

	inline uint64_t NowSinceEpoch() const
	{
		return std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()
		).count() - epoch_;
	}

	inline Guid Generate()
	{
		uint64_t time_now = NowSinceEpoch();

		if (time_now < last_time_) {
			LOG_ERROR << "系统时钟回拨：" << last_time_ << " -> " << time_now;
			time_now = GetNextTimeTick(); // 等待时间恢复
		}

		if (time_now > last_time_) {
			step_ = 0;
			last_time_ = time_now;
			Incremented();
		}
		else {
			Incremented();
		}

		return (last_time_ << kTimeShift) |
			(node_ << kNodeShift) |
			step_;
	}

private:
	uint64_t GetNextTimeTick()
	{
		uint64_t time_now = NowSinceEpoch();
		while (time_now <= last_time_) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 1ms等待
			time_now = NowSinceEpoch();
		}
		return time_now;
	}

	inline void Incremented()
	{
		step_ = (step_ + 1) & kStepMask;
		if (step_ != 0) {
			return;
		}

		// 当前秒内序列号耗尽，等待下一个时间戳
		uint64_t next_time = GetNextTimeTick();
		step_ = (step_ + 1) & kStepMask;
		last_time_ = next_time;
	}

private:
	uint64_t node_id_{ 0 };   // 节点ID原始值
	uint64_t node_{ 0 };      // 掩码处理后的节点值
	uint64_t last_time_{ 0 }; // 上一次生成ID时的时间戳
	uint64_t step_{ 0 };      // 当前秒内的步进号
	uint64_t epoch_{ kEpoch }; // 自定义时间起点
};

class SnowFlakeThreadSafe : public SnowFlake
{
public:
	typedef std::mutex MutexLock;
	typedef std::unique_lock<MutexLock> MutexLockGuard;

	SnowFlakeThreadSafe(const SnowFlakeThreadSafe&) = delete;
	SnowFlakeThreadSafe& operator=(const SnowFlakeThreadSafe&) = delete;

	SnowFlakeThreadSafe() {}

	Guid Generate()
	{
		MutexLockGuard m(mutex_);
		return SnowFlake::Generate();
	}

private:
	mutable MutexLock mutex_;
};
