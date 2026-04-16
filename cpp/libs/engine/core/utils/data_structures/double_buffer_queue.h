#pragma once

#include "muduo/base/Mutex.h"
#include "muduo/base/noncopyable.h"

#include <atomic>
#include <cstddef>
#include <utility>
#include <vector>

// Project-owned double-buffer queue.
//
// Characteristics:
// - multi-producer, single-consumer
// - non-blocking take()
// - consumer can process a fixed number of items per frame
// - read side is lock-free in the steady state
//
// Typical usage:
//   DoubleBufferQueue<Task> queue;
//
//   // producer threads
//   queue.put(task);
//
//   // main loop / frame tick
//   constexpr size_t kMaxPerFrame = 64;
//   for (size_t i = 0; i < kMaxPerFrame; ++i)
//   {
//       Task task;
//       if (!queue.take(task))
//       {
//           break;
//       }
//       task();
//   }
template <typename T>
class DoubleBufferQueue : noncopyable
{
public:
    using queue_type = std::vector<T>;

    DoubleBufferQueue() = default;

    void put(const T& x)
    {
        muduo::MutexLockGuard lock(mutex_);
        buffers_[writeIndex_].push_back(x);
        pendingSize_.fetch_add(1, std::memory_order_relaxed);
    }

    void put(T&& x)
    {
        muduo::MutexLockGuard lock(mutex_);
        buffers_[writeIndex_].push_back(std::move(x));
        pendingSize_.fetch_add(1, std::memory_order_relaxed);
    }

    // Non-blocking. Returns false immediately when no item is available.
    // Single-consumer only.
    bool take(T& out)
    {
        if (readPos_ >= buffers_[readIndex_].size())
        {
            if (!swapReadBuffer())
            {
                return false;
            }
        }

        out = std::move(buffers_[readIndex_][readPos_]);
        ++readPos_;
        pendingSize_.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }

    size_t size() const
    {
        return pendingSize_.load(std::memory_order_relaxed);
    }

    bool empty() const
    {
        return size() == 0;
    }

private:
    bool swapReadBuffer()
    {
        buffers_[readIndex_].clear();
        readPos_ = 0;

        muduo::MutexLockGuard lock(mutex_);
        if (buffers_[writeIndex_].empty())
        {
            return false;
        }

        std::swap(readIndex_, writeIndex_);
        return true;
    }

private:
    mutable muduo::MutexLock mutex_;
    queue_type buffers_[2];
    int writeIndex_ GUARDED_BY(mutex_) = 0;

    // Consumer-thread only.
    int readIndex_ = 1;
    size_t readPos_ = 0;

    std::atomic<size_t> pendingSize_{0};
};
