#include "timer_task.h"

#include "muduo/net/TimerId.h"
#include "muduo/net/Timer.h"

#define  gs_thread_even_loop EventLoop::getEventLoopOfCurrentThread()

TimerTask::~TimerTask()
{
    Cancel();
}

void TimerTask::RunAt(const Timestamp& time, const TimerCallback& cb)
{
	Cancel();
    callback_ = cb;
	id_ = gs_thread_even_loop->runAt(time, std::bind(&TimerTask::OnTimer, this));
    UpdateEndStamp();
}

void TimerTask::RunAfter(double delay, const TimerCallback& cb)
{
	Cancel();
    callback_ = cb;
    id_ = gs_thread_even_loop->runAfter(delay, std::bind(&TimerTask::OnTimer, this));
    UpdateEndStamp();
}

void TimerTask::RunEvery(double interval, const TimerCallback& cb)
{
    Cancel();	
    callback_ = cb;
    id_ = gs_thread_even_loop->runEvery(interval, std::bind(&TimerTask::OnTimer, this));
    UpdateEndStamp();
}

void TimerTask::Call()
{
    if (!callback_)
    {
        return;
    }
    callback_();
}

void TimerTask::Cancel()
{
    gs_thread_even_loop->cancel(id_);
    id_ = TimerId();
    end_time_ = Timestamp();
    assert(nullptr == id_.GetTimer());
}

bool TimerTask::Actived()
{
    return !(end_time_.invalid() == end_time_);
}

int32_t TimerTask::GetEndTime()
{
    if (end_time_ < Timestamp::now() )
    {
        return 0;
    }
    return (int32_t)id_.GetTimer()->expiration().secondsSinceEpoch();
}

void TimerTask::UpdateEndStamp()
{
    if (nullptr == id_.GetTimer())
    {
        return;
    }
    end_time_ = id_.GetTimer()->expiration();
}

void TimerTask::SetCallBack(const TimerCallback& cb)
{
    callback_ = cb;
}

void TimerTask::OnTimer()
{
    if (!callback_)
    {
        return;
    }
    TimerCallback copycb = callback_;
    copycb();
    end_time_ = Timestamp();
}
