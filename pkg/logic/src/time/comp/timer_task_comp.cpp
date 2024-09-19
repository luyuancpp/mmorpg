#include "timer_task_comp.h"

#include "muduo/net/Timer.h"
#include "muduo/net/TimerId.h"
#include "time/util/time_util.h"

#define  tlsEventLoop EventLoop::getEventLoopOfCurrentThread()

TimerTaskComp::~TimerTaskComp()
{
    Cancel();
}

TimerTaskComp::TimerTaskComp(const TimerTaskComp& param)
{
    const_cast<TimerTaskComp&>(param).Cancel();
}

TimerTaskComp::TimerTaskComp( TimerTaskComp&& param) noexcept
{
    param.Cancel();
}

TimerTaskComp & TimerTaskComp::operator=( TimerTaskComp&& param)
{
    param.Cancel();
    return *this;
}

void TimerTaskComp::RunAt(const Timestamp& time, const TimerCallback& cb)
{
	Cancel();
    callback_ = cb;
	id_ = tlsEventLoop->runAt(time, std::bind(&TimerTaskComp::OnTimer, this));
    UpdateEndStamp();
}

void TimerTaskComp::RunAfter(double delay, const TimerCallback& cb)
{
	Cancel();
    callback_ = cb;
    id_ = tlsEventLoop->runAfter(delay, std::bind(&TimerTaskComp::OnTimer, this));
    UpdateEndStamp();
}

void TimerTaskComp::RunEvery(double interval, const TimerCallback& cb)
{
    Cancel();	
    callback_ = cb;
    id_ = tlsEventLoop->runEvery(interval, std::bind(&TimerTaskComp::OnTimer, this));
    UpdateEndStamp();
}

void TimerTaskComp::Call()
{
    if (!callback_)
    {
        return;
    }
    callback_();
}

void TimerTaskComp::Cancel()
{
    tlsEventLoop->cancel(id_);
    id_ = TimerId();
    endTime = Timestamp();
    assert(nullptr == id_.GetTimer());
}

bool TimerTaskComp::IsActive() const
{
    return !(endTime.invalid() == endTime);
}

uint64_t TimerTaskComp::GetEndTime()
{
    if (endTime < Timestamp(TimeUtil::NowSecondsUTC()))
    {
        return 0;
    }
    return id_.GetTimer()->expiration().secondsSinceEpoch();
}

void TimerTaskComp::UpdateEndStamp()
{
    if (nullptr == id_.GetTimer())
    {
        return;
    }
    endTime = id_.GetTimer()->expiration();
}

void TimerTaskComp::SetCallBack(const TimerCallback& cb)
{
    callback_ = cb;
}

void TimerTaskComp::OnTimer()
{
    if (!callback_)
    {
        return;
    }
    TimerCallback copycb = callback_;
    copycb();
    endTime = Timestamp();
}
