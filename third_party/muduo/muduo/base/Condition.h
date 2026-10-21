// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_CONDITION_H
#define MUDUO_BASE_CONDITION_H

#include "muduo/base/Mutex.h"

#include <pthread.h>
#include <condition_variable>
#include <chrono>

namespace muduo
{
#ifdef __muduo_asynchronization__
class Condition : noncopyable
{
 public:
  explicit Condition(MutexLock& mutex)
    : mutex_(mutex)
  {
    MCHECK(pthread_cond_init(&pcond_, NULL));
  }

  ~Condition()
  {
    MCHECK(pthread_cond_destroy(&pcond_));
  }

  void wait()
  {
    MutexLock::UnassignGuard ug(mutex_);
    MCHECK(pthread_cond_wait(&pcond_, mutex_.getPthreadMutex()));
  }

  // returns true if time out, false otherwise.
  bool waitForSeconds(double seconds);

  void notify()
  {
    MCHECK(pthread_cond_signal(&pcond_));
  }

  void notifyAll()
  {
    MCHECK(pthread_cond_broadcast(&pcond_));
  }

 private:
  MutexLock& mutex_;
  pthread_cond_t pcond_;
};
#else
class Condition : noncopyable
{
public:
    explicit Condition(MutexLock& mutex)
        : mutex_(mutex)
    {
    }

    void wait()
    {
        condition_.wait(mutex_);
    }

    // returns true if time out, false otherwise.
    bool waitForSeconds(double seconds)
    {
        return condition_.wait_for(mutex_, std::chrono::duration<double>(seconds))
            == std::cv_status::timeout;
    }

    void notify()
    {
        condition_.notify_one();
    }

    void notifyAll()
    {
        condition_.notify_all();
    }

private:
    // 外层 MutexLockGuard 始终持有同一把锁;wait 只临时释放并重新获取它。
    MutexLock& mutex_;
    std::condition_variable_any condition_;
};
#endif//__muduo_asynchronization__

}  // namespace muduo

#endif  // MUDUO_BASE_CONDITION_H
