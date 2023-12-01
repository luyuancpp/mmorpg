#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

#include "muduo/base/Logging.h"

#include "src/type_define/type_define.h"

//https://dev.mysql.com/doc/refman/8.0/en/miscellaneous-functions.html#function_uuid

//https://blog.csdn.net/itlijinping_zhang/article/details/122414255
//https://github.com/bwmarrin/snowflake
//https://en.cppreference.com/w/cpp/chrono/duration second time byte
//total bit 63 for lua 
static constexpr uint64_t kEpoch = 1688298072;
static constexpr uint64_t kNodeBits = 14;
static constexpr uint64_t kStepBits = 14;
static constexpr uint64_t kTimeByte = 35;
static constexpr uint64_t kMaxIncremented = UINT16_MAX;
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
        node_ = node_id;
        node_ = (node_ << kTimeByte) << kStepBits;
    }
    inline uint16_t node_id()const { return node_id_; }

    inline time_t NowSinceEpoch() const
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()
            - kEpoch;
    }

    inline Guid Generate()
    {
        const uint64_t time_now = NowSinceEpoch();
        if (time_now > last_time_)
        {
            step_ = 0;
            last_time_ = time_now;
            Incremented();
        }
        else
        {
            //log error if diff max 1 s
            Incremented();
        }

        return (last_time_ << kTimeShift) |
            (node_ << kNodeShift) |
            step_;
    }
private:
    inline void Incremented()
    {
        step_ = (step_ + 1) & kStepMask;
        if (step_ != 0)
        {
            return;
        }
        step_ = (step_ + 1) & kStepMask;
        // arrive current seconds max id ,use next time id
        ++last_time_;
    }

    uint16_t node_id_{ 0 };
    uint64_t node_{ 0 };
    uint64_t last_time_{ 0 };
    uint32_t step_{ 0 };
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



//服务器重启以后失效的
template <size_t kNodeBit>
class ServerSequence
{
public:
    static size_t node_bit() { return kNodeBit; }
    
void set_node_id(uint64_t node_id)
{
    node_id_ = node_id << kNodeBit;
}

uint32_t node_id(Guid guid)
{
    return guid >> kNodeBit;
}

Guid Generate()
{
	return node_id_ | ++seq_;
}

//for test
Guid LastId()
{
    return  node_id_ | seq_;
}
private:
uint64_t node_id_{ 0 };
uint64_t seq_{ 0 };
};

using ServerSequence16 = ServerSequence<16>;
using ServerSequence24 = ServerSequence<24>;
using ServerSequence32 = ServerSequence<32>;

