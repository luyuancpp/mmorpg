// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_TIMERQUEUE_H
#define MUDUO_NET_TIMERQUEUE_H

#include <set>
#include <vector>

#include "muduo/base/Mutex.h"
#include "muduo/base/Timestamp.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/Channel.h"

namespace muduo
{
namespace net
{

class EventLoop;
class Timer;
class TimerId;

///
/// A best efforts timer queue.
/// No guarantee that the callback will be on time.
///
/// 本仓库改动(上游按墙钟 Timestamp::now() 到期):队列里的到期时刻、上弦时长与重复定时器的
/// 下一次到期一律在单调时钟域(见 monotonicNow()),墙钟回拨 / 前跳不改变定时器何时到期。
///
class TimerQueue : noncopyable
{
 public:
  explicit TimerQueue(EventLoop* loop);
  ~TimerQueue();

  ///
  /// Schedules the callback to be run at given time,
  /// repeats if @c interval > 0.0.
  ///
  /// Must be thread safe. Usually be called from other threads.
  ///
  /// @c when 是单调时钟域的时刻(monotonicNow() 的读数加上时长),不是 epoch 时间;
  /// 墙钟时刻先用 monotonicFromWall() 换算。EventLoop::runAt / runAfter / runEvery 已经代办。
  TimerId addTimer(TimerCallback cb,
                   Timestamp when,
                   double interval);

  void cancel(TimerId timerId);

  void loop();

  // Milliseconds until the earliest pending timer is due, clamped to
  // [0, fallbackMs]; fallbackMs when no timer is pending.
  //
  // Windows has no timerfd, so nothing in the poller fires when a timer comes
  // due -- EventLoop::loop() polls with a constant timeout and only then calls
  // loop() above to sweep the queue. With a constant, every timer is rounded up
  // to that poll period (measured: RunEvery(0.01) actually ticked at ~108ms).
  // Feeding this value to the poller instead is what libevent/libuv/asio do on
  // platforms without timerfd, and restores millisecond-ish granularity.
  int nextTimeoutMs(int fallbackMs) const;

  // 定时器时钟(本仓库改动)。
  //
  // 到期判断与上弦一律按单调时钟(std::chrono::steady_clock),与 POSIX timerfd 的 CLOCK_MONOTONIC、
  // asio::steady_timer 同一口径;墙钟(Timestamp::now())只用于对外显示。上游按墙钟到期:墙钟回拨 N 秒,
  // 所有定时器(调用截止、建连、探测、业务看门狗)都晚 N 秒触发;墙钟前跳则提前触发。
  //
  // 读数借 Timestamp 承载,以复用 addTime / operator< 和 Timer 的现有字段,但它不是 epoch 时间:
  // 原点是系统启动附近一个未指定的时刻,只能与同一时钟的读数相减、比较;不能格式化成日期,
  // 不能与 Timestamp::now() 混算;由很久以前的墙钟时刻换算来的值可以 <= 0,valid() 对它没有意义。
  static Timestamp monotonicNow();

  // 把墙钟时刻 wallTime 换算到单调域:monoNow + (wallTime - wallNow)。wallNow / monoNow 由调用方在
  // 同一时刻附近读取(Timestamp::now() 与 monotonicNow());纯函数,便于单测。
  // 只在挂定时器时换算一次:之后墙钟再跳,定时器仍在原定的时长之后到期,不跟随墙钟。
  static Timestamp monotonicFromWall(Timestamp wallTime, Timestamp wallNow, Timestamp monoNow);

 private:

  // FIXME: use unique_ptr<Timer> instead of raw pointers.
  // This requires heterogeneous comparison lookup (N3465) from C++14
  // so that we can find an T* in a set<unique_ptr<T>>.
  typedef std::pair<Timestamp, Timer*> Entry;
  typedef std::set<Entry> TimerList;
  typedef std::pair<Timer*, int64_t> ActiveTimer;
  typedef std::set<ActiveTimer> ActiveTimerSet;

  void addTimerInLoop(Timer* timer);
  void cancelInLoop(TimerId timerId);
  // called when timerfd alarms
  void handleRead();
  // move out all expired timers
  std::vector<Entry> getExpired(Timestamp now);
  void reset(const std::vector<Entry>& expired, Timestamp now);

  bool insert(Timer* timer);

  EventLoop* loop_;
  const int timerfd_;
  Channel timerfdChannel_;
  // Timer list sorted by expiration
  TimerList timers_;

  // for cancel()
  ActiveTimerSet activeTimers_;
  bool callingExpiredTimers_; /* atomic */
  ActiveTimerSet cancelingTimers_;
};

}  // namespace net
}  // namespace muduo
#endif  // MUDUO_NET_TIMERQUEUE_H
