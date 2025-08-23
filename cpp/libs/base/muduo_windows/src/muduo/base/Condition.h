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
class Condition : public std::condition_variable
{
public:

    // returns true if time out, false otherwise.
    bool waitForSeconds(MutexLockGuard & lock, double seconds)
    {
        wait_for(lock, std::chrono::seconds((int32_t)seconds));
        return true;
    }

    void notify()
    {
        notify_all();
    }

    void notifyAll()
    {
        notify_all();
    }

private:
 
};
#endif//__muduo_asynchronization__

}  // namespace muduo

#endif  // MUDUO_BASE_CONDITION_H
