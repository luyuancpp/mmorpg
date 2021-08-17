#ifndef SRC_SNOW_FLAKE_SNOW_FLAKE_H_
#define SRC_SNOW_FLAKE_SNOW_FLAKE_H_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

#include "src/common_type/common_type.h"

//https://dev.mysql.com/doc/refman/8.0/en/miscellaneous-functions.html#function_uuid

namespace common
{
    class SnowFlake
    {
    public:
        //https://en.cppreference.com/w/cpp/chrono/duration second time byte
        static const uint32_t kServerIdByte = 16;
        static const uint32_t kTimeByte = 32;
        static const uint32_t kIncrementedByte = 16;
        static const uint32_t kMaxIncremented = UINT16_MAX;

        typedef std::mutex MutexLock;
        typedef std::unique_lock<MutexLock> MutexLockGuard;

        SnowFlake(const SnowFlake&) = delete;
        SnowFlake& operator=(const SnowFlake&) = delete;

        explicit SnowFlake()
            :server_id_(0),
            server_id_flag_(0)
        {
        }

        void set_serverId(uint16_t server_id)
        {
            server_id_ = server_id;
            server_id_flag_ = server_id;
            server_id_flag_ = (server_id_flag_ << kTimeByte) << kIncrementedByte;
        }
        uint16_t server_id()const { return server_id_; }

        time_t GetNow()
        {
            return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

        GameGuid Generate()
        {
            uint32_t sequence = 0;
            uint64_t time_bit = 0;
            {
                uint64_t time_now = GetNow();
                MutexLockGuard m(mutex_);                
                if (time_now > last_time_)
                {
                    sequence_ = 0;
                    last_time_ = time_now;
                }
                else if (time_now <= last_time_)
                {
                    ResetIncremented();
                }

                sequence = sequence_ + 1;
                ++sequence_;
                time_bit = last_time_;
            }
              
            return server_id_flag_ + (time_bit << kIncrementedByte) + sequence;
        }
    private:
        inline void ResetIncremented() 
        {
            // arrive current seconds max id ,use next id
            if (sequence_ + 1 < kMaxIncremented)
            {
                return;
            }
            sequence_ = 0;
            ++last_time_;            
        }

    uint16_t server_id_{ 0 };
    uint64_t server_id_flag_{ 0 };   
    uint64_t last_time_{ 0 };
    uint32_t sequence_{ 0 };
    mutable MutexLock mutex_;
};
}//namespace common


#endif // SRC_SNOW_FLAKE_SNOW_FLAKE_H_