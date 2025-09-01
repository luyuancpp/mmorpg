// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREAD_H
#define MUDUO_BASE_THREAD_H

#include "muduo/base/Atomic.h"
#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Types.h"

#include <functional>
#include <memory>
#include <pthread.h>

#include <thread>
namespace muduo
{
#ifdef __muduo_asynchronization__
class Thread : noncopyable
{
 public:
  typedef std::function<void ()> ThreadFunc;

  explicit Thread(ThreadFunc, const string& name = string());
  // FIXME: make it movable in C++11
  ~Thread();

  void start();
  int join(); // return pthread_join()

  bool started() const { return started_; }
  // pthread_t pthreadId() const { return pthreadId_; }
  pid_t tid() const { return tid_; }
  const string& name() const { return name_; }

  static int numCreated() { return numCreated_.get(); }

 private:
  void setDefaultName();

  bool       started_;
  bool       joined_;
  pthread_t  pthreadId_;
  pid_t      tid_;
  ThreadFunc func_;
  string     name_;
  CountDownLatch latch_;

  static AtomicInt32 numCreated_;
};
#else 
class Thread : noncopyable
{
public:
    typedef std::function<void()> ThreadFunc;

    explicit Thread(ThreadFunc func, const string& name = string())
        : func_(func),
          name_(name)
    {
        numCreated_.incrementAndGet();
    }
    // FIXME: make it movable in C++11
    ~Thread() {if (started())thread_.join();}

    void start() { std::thread t(func_); thread_.swap(t); }
    int join() { thread_.join(); return 1; }

    bool started() const { return thread_.joinable(); }
    const string& name() const { return name_; }

    static int numCreated() { return numCreated_.get(); }

private:
    ThreadFunc func_;
    string     name_;
    static AtomicInt32 numCreated_;
    std::thread thread_;
};
#endif//__muduo_asynchronization__
}  // namespace muduo
#endif  // MUDUO_BASE_THREAD_H
