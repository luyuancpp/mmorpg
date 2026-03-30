#include "timer_task_comp.h"
#include "muduo/net/Timer.h"
#include "muduo/net/TimerId.h"
#include "time/system/time.h"

static inline muduo::net::EventLoop* GetThreadEventLoop() {
    return muduo::net::EventLoop::getEventLoopOfCurrentThread();
}

TimerTaskComp::TimerTaskComp() {
}

TimerTaskComp::~TimerTaskComp() {
    Cancel();
}

TimerTaskComp::TimerTaskComp(const TimerTaskComp& param) {
    const_cast<TimerTaskComp&>(param).Cancel();
}

TimerTaskComp::TimerTaskComp(TimerTaskComp&& param) noexcept {
    param.Cancel();
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
    timerId = GetThreadEventLoop()->runAt(time, std::bind(&TimerTaskComp::OnTimer, this));
}

void TimerTaskComp::RunAfter(const double delay, const TimerCallback& cb) {
    Cancel();
    callback = cb;
    timerId = GetThreadEventLoop()->runAfter(delay, std::bind(&TimerTaskComp::OnTimer, this));
}

void TimerTaskComp::RunEvery(const double interval, const TimerCallback& cb) {
    Cancel();
    callback = cb;
    timerId = GetThreadEventLoop()->runEvery(interval, std::bind(&TimerTaskComp::OnTimer, this));
}

void TimerTaskComp::Run() const
{
    if (callback) {
        callback();
    }
}

void TimerTaskComp::Cancel() {
    auto* loop = GetThreadEventLoop();
    if (loop != nullptr) {
        loop->cancel(timerId);
    }
    timerId = TimerId();
    callback = TimerCallback();
    assert(timerId.GetTimer() == nullptr);
}


bool TimerTaskComp::IsActive() {
	auto timer = timerId.GetTimer();
	if (!timer) {
		return false;
	}

	auto expiration = timer->expiration();
	return expiration != Timestamp::invalid() && !(expiration < Timestamp::now());
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
