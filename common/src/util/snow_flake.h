#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

#include "muduo/base/Logging.h"

#include "src/common_type/common_type.h"

//https://dev.mysql.com/doc/refman/8.0/en/miscellaneous-functions.html#function_uuid

//https://blog.csdn.net/itlijinping_zhang/article/details/122414255
//https://github.com/bwmarrin/snowflake
//https://en.cppreference.com/w/cpp/chrono/duration second time byte
//total bit 63 for lua 
static constexpr uint64_t kEpoch = 1688298072000;
static constexpr uint64_t kNodeBits = 14;
static constexpr uint64_t kStepBits = 14;
static constexpr uint64_t kTimeByte = 35;
static constexpr uint64_t kMaxIncremented = UINT16_MAX;
static constexpr uint64_t kTimeShift = kNodeBits + kStepBits;
static constexpr uint64_t kNodeShift = kStepBits;
static constexpr uint64_t kStepMask = -1 ^ (-1 << kStepBits);

//Ê±ÖÓ»Øµ÷

class SnowFlakeThreadSafe
{
public:
    typedef std::mutex MutexLock;
    typedef std::unique_lock<MutexLock> MutexLockGuard;

    SnowFlakeThreadSafe(const SnowFlakeThreadSafe&) = delete;
    SnowFlakeThreadSafe& operator=(const SnowFlakeThreadSafe&) = delete;

    explicit SnowFlakeThreadSafe()
        :node_id_(0),
        node_(0)
    {
    }

    void set_node_id(uint16_t node_id)
    {
        node_id_ = node_id;
        node_ = node_id;
        node_ = (node_ << kTimeByte) << kStepBits;
    }
    uint16_t node_id()const { return node_id_; }

    time_t NowSinceEpoch() const
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()
        - kEpoch;
    }

    Guid Generate()
    {
        uint32_t step = 0;
        uint64_t time = 0;
        {
            MutexLockGuard m(mutex_);
            const uint64_t time_now = NowSinceEpoch();
            if (time_now > last_time_)
            {
                step_ = 0;
                last_time_ = time_now;
                Incremented();
            }
            else if (time_now == last_time_)
            {
                Incremented();
            }
            else
            {
                //log error if diff max 1 s
                Incremented();
            }
            step = step_;
            time = last_time_;
        }
              
        return (time << kTimeShift) |
                (node_ << kNodeShift) |
                step;
    }
private:
    inline void Incremented() 
    {
        step_ = (step_ + 1) & kStepMask;
        if (step_ != 0)
        {
            return;
        }
        assert(step_ == 0);
        step_ = (step_ + 1) & kStepMask;
        // arrive current seconds max id ,use next time id
        ++last_time_;            
    }

uint16_t node_id_{ 0 };
uint64_t node_{ 0 };   
uint64_t last_time_{ 0 };
uint32_t step_{ 0 };
mutable MutexLock mutex_;
};

class SnowFlake
{
public:
    SnowFlake(const SnowFlake&) = delete;
    SnowFlake& operator=(const SnowFlake&) = delete;

    explicit SnowFlake()
        :node_id_(0),
        node_(0)
    {
    }

    void set_node_id(uint16_t node_id)
    {
        node_id_ = node_id;
        node_ = node_id;
        node_ = (node_ << kTimeByte) << kStepBits;
    }
    uint16_t node_id()const { return node_id_; }

    time_t NowSinceEpoch() const
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()
            - kEpoch;
    }

    Guid Generate()
    {
        uint32_t step = 0;
        uint64_t time = 0;
        {
            const uint64_t time_now = NowSinceEpoch();
            if (time_now > last_time_)
            {
                step_ = 0;
                last_time_ = time_now;
                Incremented();
            }
            else if (time_now == last_time_)
            {
                Incremented();
            }
            else
            {
                //log error if diff max 1 s
                Incremented();
            }
            step = step_;
            time = last_time_;
        }

        return (time << kTimeShift) |
            (node_ << kNodeShift) |
            step;
    }
private:
    inline void Incremented()
    {
        step_ = (step_ + 1) & kStepMask;
        if (step_ != 0)
        {
            return;
        }
        assert(step_ == 0);
        step_ = (step_ + 1) & kStepMask;
        // arrive current seconds max id ,use next time id
        ++last_time_;
    }

    uint16_t node_id_{ 0 };
    uint64_t node_{ 0 };
    uint64_t last_time_{ 0 };
    uint32_t step_{ 0 };
};

//服务器重启以后失效的
template <size_t kNodeBit>
class ServerSequence
{
public:
static const uint8_t kNodeBit = 32;

void set_node_id(uint32_t node_id)
{
	node_id_ = node_id;
    node_id_ = node_id_ << kNodeBit;
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

