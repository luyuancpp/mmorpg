#include "timer_task_comp.h"
#include "muduo/net/Timer.h"
#include "muduo/net/TimerId.h"
#include "time/system/time_system.h"

#define tlsEventLoop EventLoop::getEventLoopOfCurrentThread()

TimerTaskComp::TimerTaskComp() {
    Cancel();
}

TimerTaskComp::~TimerTaskComp() {
    Cancel();
}

TimerTaskComp::TimerTaskComp(const TimerTaskComp& param) {
    const_cast<TimerTaskComp&>(param).Cancel();
    Cancel();
}

TimerTaskComp::TimerTaskComp(TimerTaskComp&& param) noexcept {
    param.Cancel();
    Cancel();
}

TimerTaskComp& TimerTaskComp::operator=(TimerTaskComp&& param) noexcept
{
    param.Cancel();
    Cancel();
    return *this;
}

void TimerTaskComp::RunAt(const Timestamp& time, const TimerCallback& cb) {
    Cancel();
    callback = cb;
    timerId = tlsEventLoop->runAt(time, std::bind(&TimerTaskComp::OnTimer, this));
}

void TimerTaskComp::RunAfter(const double delay, const TimerCallback& cb) {
    Cancel();
    callback = cb;
    timerId = tlsEventLoop->runAfter(delay, std::bind(&TimerTaskComp::OnTimer, this));
}

void TimerTaskComp::RunEvery(const double interval, const TimerCallback& cb) {
    Cancel();
    callback = cb;
    timerId = tlsEventLoop->runEvery(interval, std::bind(&TimerTaskComp::OnTimer, this));
}

void TimerTaskComp::Run() const
{
    if (callback) {
        callback();
    }
}

void TimerTaskComp::Cancel() {
    tlsEventLoop->cancel(timerId);
    timerId = TimerId();
    callback = TimerCallback();
    assert(nullptr == timerId.GetTimer());
}

bool TimerTaskComp::IsActive() {
    if (nullptr == timerId.GetTimer())
    {
        return false;
    }

    return Timestamp::invalid() != timerId.GetTimer()->expiration() && timerId.GetTimer()->expiration() < Timestamp::now();
}

uint64_t TimerTaskComp::GetEndTime() {
    return timerId.GetTimer()->expiration().secondsSinceEpoch();
}

void TimerTaskComp::SetCallBack(const TimerCallback& cb) {
    callback = cb;
}

void TimerTaskComp::OnTimer() {
    if (callback) {
        const TimerCallback copycb = callback;
        copycb();
    }
}
