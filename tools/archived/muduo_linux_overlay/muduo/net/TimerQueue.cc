// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

// ---------------------------------------------------------------------------
// LOCAL OVERLAY -- copied over third_party/muduo-linux/muduo/net/TimerQueue.cc
// by tools/archived/setup_dependencies.sh before muduo is configured.
//
// Verbatim upstream except for the blocks marked LOCAL DELTA below.
//
// (1) One guard in handleRead(): a timer cancelled by an earlier callback in
// the same expired batch must not run, because cancelling almost always means
// its owner is being destroyed. Windows already carries this fix
// (cpp/libs/engine/muduo_windows/src/muduo/net/TimerQueue.cc, commit e137bd658);
// this is the Linux half, which was missing.
//
// (2) 定时器改按单调时钟到期(与 Windows 同一改动):monotonicNow() / monotonicFromWall()
// 的实现;howMuchTimeFromNow() 与 handleRead() 的 now 改取单调读数;reset() 不再用
// valid() 判断"还有没有下一个"。声明在同目录 TimerQueue.h 覆盖件,调用方换算在同目录
// EventLoop.cc 覆盖件 —— 三个文件必须一起生效,单独覆盖任何一个都会让时钟域错配。
//
// (3) 闭包析构可重入 cancel:主动取消与队列销毁先摘除两个索引再释放 Timer,
// 整批到期 Timer 清理完成后才退出取消窗口,避免自删、索引不一致和重复 Timer 重新入队。
//
// It lives here rather than in the submodule because third_party/muduo-linux
// is chenshuo/muduo -- only its *pointer* is committed, so an edit to its
// working tree would not survive a fresh clone. Keeping the delta in this
// repository does, and leaves the upstream checkout pristine.
//
// Re-sync procedure when bumping the muduo submodule: copy the new upstream
// file over this one and re-apply the LOCAL DELTA block.
// ---------------------------------------------------------------------------

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include "muduo/net/TimerQueue.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/Timer.h"
#include "muduo/net/TimerId.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include <chrono>  // LOCAL DELTA (2)

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
  // LOCAL DELTA (2):when 在单调域,剩余时长也按单调时钟算(timerfd 本身就是 CLOCK_MONOTONIC、按相对时长上弦)。
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
  uint64_t howmany;
  ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
  // LOCAL DELTA (2):now 是单调读数(秒.微秒,原点约为开机时刻),不是日期。
  LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at monotonic " << now.toString();
  if (n != sizeof howmany)
  {
    LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
  }
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

// LOCAL DELTA (2):单调时钟读数与墙钟换算,语义见 TimerQueue.h。
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
  ::close(timerfd_);
  // do not remove channel, since we're in EventLoop::dtor();
  // LOCAL DELTA (3):闭包析构可能取消自己或另一只待销毁 Timer,不保留可被重入失效的迭代器。
  while (!timers_.empty())
  {
    Timer* timer = timers_.begin()->second;
    timers_.erase(timers_.begin());
    activeTimers_.erase(ActiveTimer(timer, timer->sequence()));
    delete timer;
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
    // LOCAL DELTA (3):两个索引先保持一致,再析构可重入 cancel 的闭包捕获对象。
    Timer* cancelledTimer = it->first;
    size_t n = timers_.erase(Entry(cancelledTimer->expiration(), cancelledTimer));
    assert(n == 1); (void)n;
    activeTimers_.erase(it);
    delete cancelledTimer;
  }
  else if (callingExpiredTimers_)
  {
    cancelingTimers_.insert(timer);
  }
  assert(timers_.size() == activeTimers_.size());
}

void TimerQueue::handleRead()
{
  loop_->assertInLoopThread();
  // LOCAL DELTA (2):到期判断(getExpired)与重复定时器的下一次到期(reset → Timer::restart)都按单调 now。
  Timestamp now(monotonicNow());
  readTimerfd(timerfd_, now);

  std::vector<Entry> expired = getExpired(now);

  callingExpiredTimers_ = true;
  cancelingTimers_.clear();
  // safe to callback outside critical section
  for (const Entry& it : expired)
  {
    // LOCAL DELTA (1).
    //
    // getExpired() already removed this batch from activeTimers_, so a
    // cancel() issued by an earlier callback in this same batch cannot take
    // the timer back -- cancelInLoop() can only record it in cancelingTimers_.
    // Upstream consults that set solely in reset() below, to stop a repeating
    // timer being re-inserted; it does not skip the pending run().
    //
    // Cancelling almost always means the callback's owner is being destroyed,
    // so running it anyway dereferences freed memory. This cannot be fixed
    // above muduo: a component-level "am I stale" guard would have to be read
    // out of the already-freed object to be consulted at all.
    //
    // find() rather than contains() so this does not depend on the C++
    // standard level muduo happens to be compiled with; reset() below uses the
    // same form.
    //
    // See docs/design/muduo-timer-cancellation-hazards.md and the regression
    // tests in cpp/tests/timer_queue_unit_test/.
    if (cancelingTimers_.find(ActiveTimer(it.second, it.second->sequence()))
        != cancelingTimers_.end())
    {
      continue;
    }
    it.second->run();
  }
  // LOCAL DELTA (3):reset 释放闭包时也可能重入 cancel 本批尚未重新入队的重复 Timer。
  reset(expired, now);
  callingExpiredTimers_ = false;
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

  // LOCAL DELTA (2):上游用 nextExpire.valid()(> 0)判断"还有没有下一个"。单调域的到期时刻可以 <= 0
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
