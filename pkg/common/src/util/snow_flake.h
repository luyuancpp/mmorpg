#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

#include "muduo/base/Logging.h"

#include "type_define/type_define.h"

//https://github.com/yitter/IdGenerator
//https://dev.mysql.com/doc/refman/8.0/en/miscellaneous-functions.html#function_uuid

//https://blog.csdn.net/itlijinping_zhang/article/details/122414255
//https://github.com/bwmarrin/snowflake
//https://en.cppreference.com/w/cpp/chrono/duration second time byte
//total bit 63 for lua 
static constexpr uint64_t kEpoch = 1719674201;
static constexpr uint64_t kNodeBits = 13;
static constexpr uint64_t kStepBits = 18;
static constexpr uint64_t kTimeByte = 32;
static constexpr uint64_t kTimeShift = kNodeBits + kStepBits;
static constexpr uint64_t kNodeShift = kStepBits;
static constexpr uint64_t kStepMask = UINT64_MAX ^ (UINT64_MAX << kStepBits);

class SnowFlake
{
public:
    SnowFlake(const SnowFlake&) = delete;
    SnowFlake& operator=(const SnowFlake&) = delete;
    SnowFlake() {}

	inline void set_node_id(uint16_t node_id)
	{
		node_id_ = node_id;
		node_ = (static_cast<uint64_t>(node_id) << kStepBits) & ((1 << kNodeBits) - 1);
	}

    inline uint64_t node_id()const { return node_id_; }

    inline void set_epoch(uint64_t epoch) { epoch_ = epoch; }

    inline uint64_t NowSinceEpoch() const
    {
        return 
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()
            - epoch_;
    }

    inline Guid Generate()
    {
        uint64_t time_now = NowSinceEpoch();
        if (time_now > last_time_)
        {
            step_ = 0;
            last_time_ = time_now;
            Incremented();
        }
        else
        {
            Incremented();
        }

        return (last_time_ << kTimeShift) |
            (node_ << kNodeShift) |
            step_;
    }
private:

    auto GetNextTimeTick()
    {
        auto time_now = NowSinceEpoch();
        while (time_now <= last_time_)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 暂停1ms
            time_now = NowSinceEpoch();
        }
        return time_now;
    }

    inline void Incremented()
    {
        step_ = (step_ + 1) & kStepMask;
        if (step_ != 0)
        {
            return;
        }
        // arrive current seconds max id ,use next time id

        auto time_now = NowSinceEpoch();
         //log error if diff max 1 s
        if (time_now > last_time_ && time_now - last_time_ > 1)
        {
            LOG_ERROR << " time back " << time_now << "," << last_time_;
        }

        uint64_t next_time = GetNextTimeTick();
        step_ = (step_ + 1) & kStepMask;
        last_time_ = next_time;
    }

    uint64_t node_id_{ 0 };
    uint64_t node_{ 0 };
    uint64_t last_time_{ 0 };
    uint64_t step_{ 0 };
    uint64_t epoch_{ kEpoch };
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






