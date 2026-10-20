// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include "muduo/net/TimerQueue.h"

#include "muduo/base/CrossPlatformAdapterFunction.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/Timer.h"
#include "muduo/net/TimerId.h"
#include "muduo/net/SocketsOps.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include <chrono>

namespace muduo
{
namespace net
{
namespace detail
{

int createTimerfd()
{
  int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                 TFD_NONBLOCK | TFD_CLOEXEC);
  if (timerfd < 0)
  {
    LOG_SYSFATAL << "Failed in timerfd_create";
  }
  return timerfd;
}

struct timespec howMuchTimeFromNow(Timestamp when)
{
  // 本仓库改动:when 在单调域,剩余时长也按单调时钟算(timerfd 本身就是 CLOCK_MONOTONIC、按相对时长上弦)。
  int64_t microseconds = when.microSecondsSinceEpoch()
                         - TimerQueue::monotonicNow().microSecondsSinceEpoch();
  if (microseconds < 100)
  {
    microseconds = 100;
  }
  struct timespec ts;
  ts.tv_sec = static_cast<time_t>(
      microseconds / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<long>(
      (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
  return ts;
}

void readTimerfd(int timerfd, Timestamp now)
{
#ifdef __linux__
  uint64_t howmany;
  ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
  // now 是单调读数(秒.微秒,原点约为开机时刻),不是日期。
  LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at monotonic " << now.toString();
  if (n != sizeof howmany)
  {
    LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
  }
#endif // __linux__
#ifdef WIN32
  (void)timerfd;
  (void)now;
#endif // WIN32
}

void resetTimerfd(int timerfd, Timestamp expiration)
{
  // wake up loop by timerfd_settime()
  struct itimerspec newValue;
  struct itimerspec oldValue;
  memZero(&newValue, sizeof newValue);
  memZero(&oldValue, sizeof oldValue);
  newValue.it_value = howMuchTimeFromNow(expiration);
  int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
  if (ret)
  {
    LOG_SYSERR << "timerfd_settime()";
  }
}

}  // namespace detail
}  // namespace net
}  // namespace muduo

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::detail;

Timestamp TimerQueue::monotonicNow()
{
  const auto sinceOrigin = std::chrono::steady_clock::now().time_since_epoch();
  return Timestamp(std::chrono::duration_cast<std::chrono::microseconds>(sinceOrigin).count());
}

Timestamp TimerQueue::monotonicFromWall(Timestamp wallTime, Timestamp wallNow, Timestamp monoNow)
{
  const int64_t offset = wallTime.microSecondsSinceEpoch() - wallNow.microSecondsSinceEpoch();
  return Timestamp(monoNow.microSecondsSinceEpoch() + offset);
}

TimerQueue::TimerQueue(EventLoop* loop)
  : loop_(loop),
    timerfd_(createTimerfd()),
    timerfdChannel_(loop, timerfd_),
    timers_(),
    callingExpiredTimers_(false)
{
  timerfdChannel_.setReadCallback(
      std::bind(&TimerQueue::handleRead, this));
  // we are always reading the timerfd, we disarm it with timerfd_settime.
  timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
  timerfdChannel_.disableAll();
  timerfdChannel_.remove();
#ifdef __linux__
  ::close(timerfd_);
#endif // __linux__
#ifdef WIN32
  ::winclosesock(timerfd_);
#endif // WIN32
  // do not remove channel, since we're in EventLoop::dtor();
  for (const Entry& timer : timers_)
  {
    delete timer.second;
  }
}

TimerId TimerQueue::addTimer(TimerCallback cb,
                             Timestamp when,
                             double interval)
{
  Timer* timer = new Timer(std::move(cb), when, interval);
  loop_->runInLoop(
      std::bind(&TimerQueue::addTimerInLoop, this, timer));
  return TimerId(timer, timer->sequence());
}

void TimerQueue::cancel(TimerId timerId)
{
  loop_->runInLoop(
      std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
  loop_->assertInLoopThread();
  bool earliestChanged = insert(timer);

  if (earliestChanged)
  {
    resetTimerfd(timerfd_, timer->expiration());
  }
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  ActiveTimer timer(timerId.timer_, timerId.sequence_);
  ActiveTimerSet::iterator it = activeTimers_.find(timer);
  if (it != activeTimers_.end())
  {
    size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
    assert(n == 1); (void)n;
    delete it->first; // FIXME: no delete please
    activeTimers_.erase(it);
  }
  else if (callingExpiredTimers_)
  {
    cancelingTimers_.insert(timer);
  }
  assert(timers_.size() == activeTimers_.size());
}

void TimerQueue::loop()
{
    handleRead();
}

int TimerQueue::nextTimeoutMs(int fallbackMs) const
{
  if (timers_.empty())
  {
    return fallbackMs;
  }

  // 到期时刻在单调域,now 也必须取单调读数(本仓库改动)。
  const int64_t microseconds = timers_.begin()->first.microSecondsSinceEpoch()
                             - monotonicNow().microSecondsSinceEpoch();
  if (microseconds <= 0)
  {
    return 0;   // already due; poll must not block
  }

  // Round up: sleeping a hair short only wakes the loop to find nothing due,
  // and it would spin until the timestamp is actually reached.
  const int64_t milliseconds = (microseconds + 999) / 1000;
  return milliseconds < fallbackMs ? static_cast<int>(milliseconds) : fallbackMs;
}
void TimerQueue::handleRead()
{
  loop_->assertInLoopThread();
  // 本仓库改动:到期判断(getExpired)与重复定时器的下一次到期(reset → Timer::restart)都按单调 now。
  Timestamp now(monotonicNow());
  readTimerfd(timerfd_, now);

  std::vector<Entry> expired = getExpired(now);

  callingExpiredTimers_ = true;
  cancelingTimers_.clear();
  // safe to callback outside critical section
  for (const Entry& it : expired)
  {
      if (cancelingTimers_.contains(ActiveTimer{ it.second, it.second->sequence() }))
      {
          continue;
      }
    it.second->run();
  }
  callingExpiredTimers_ = false;

  reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
  assert(timers_.size() == activeTimers_.size());
  std::vector<Entry> expired;
  Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
  TimerList::iterator end = timers_.lower_bound(sentry);
  assert(end == timers_.end() || now < end->first);
  std::copy(timers_.begin(), end, back_inserter(expired));
  timers_.erase(timers_.begin(), end);

  for (const Entry& it : expired)
  {
    ActiveTimer timer(it.second, it.second->sequence());
    size_t n = activeTimers_.erase(timer);
    assert(n == 1); (void)n;
  }

  assert(timers_.size() == activeTimers_.size());
  return expired;
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
  for (const Entry& it : expired)
  {
    ActiveTimer timer(it.second, it.second->sequence());
    if (it.second->repeat()
        && cancelingTimers_.find(timer) == cancelingTimers_.end())
    {
      it.second->restart(now);
      insert(it.second);
    }
    else
    {
      // FIXME move to a free list
      delete it.second; // FIXME: no delete please
    }
  }

  // 本仓库改动:上游用 nextExpire.valid()(> 0)判断"还有没有下一个"。单调域的到期时刻可以 <= 0
  // (很久以前的墙钟时刻换算而来),valid() 不再等价于"队列非空",直接看队列。
  if (!timers_.empty())
  {
    resetTimerfd(timerfd_, timers_.begin()->second->expiration());
  }
}

bool TimerQueue::insert(Timer* timer)
{
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  bool earliestChanged = false;
  Timestamp when = timer->expiration();
  TimerList::iterator it = timers_.begin();
  if (it == timers_.end() || when < it->first)
  {
    earliestChanged = true;
  }
  {
    std::pair<TimerList::iterator, bool> result
      = timers_.insert(Entry(when, timer));
    assert(result.second); (void)result;
  }
  {
    std::pair<ActiveTimerSet::iterator, bool> result
      = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
    assert(result.second); (void)result;
  }

  assert(timers_.size() == activeTimers_.size());
  return earliestChanged;
}

