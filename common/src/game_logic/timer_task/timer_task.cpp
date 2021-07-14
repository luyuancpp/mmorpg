#include "timer_task.h"

#include "muduo/net/TimerId.h"
#include "muduo/net/Timer.h"


namespace common
{
#define  gs_thread_even_loop EventLoop::getEventLoopOfCurrentThread()

	TimerTask::~TimerTask()
	{
        Cancel();
	}

	void TimerTask::RunAt(const Timestamp& time, const TimerCallback& cb)
	{
		Cancel();
        timer_function_callback_ = cb;
		timer_id_ = gs_thread_even_loop->runAt(time, std::bind(&TimerTask::OnTimer, this));
        UpdateEndTeamDate();
	}

	void TimerTask::RunAfter(double delay, const TimerCallback& cb)
	{
		Cancel();
        timer_function_callback_ = cb;
        timer_id_ = gs_thread_even_loop->runAfter(delay, std::bind(&TimerTask::OnTimer, this));
        UpdateEndTeamDate();
	}

	void TimerTask::RunEvery(double interval, const TimerCallback& cb)
	{
        Cancel();	
        timer_function_callback_ = cb;
        timer_id_ = gs_thread_even_loop->runEvery(interval, std::bind(&TimerTask::OnTimer, this));
        UpdateEndTeamDate();
	}

    void TimerTask::Call()
    {
        if (!timer_function_callback_)
        {
            return;
        }
        timer_function_callback_();
    }

	void TimerTask::Cancel()
	{
        gs_thread_even_loop->cancel(timer_id_);
        timer_id_ = TimerId();
        end_timestamp_ = Timestamp();
        assert(nullptr == timer_id_.GetTimer());
	}

    bool TimerTask::ActiveTimer()
    {
        return !(end_timestamp_.invalid() == end_timestamp_);
    }

    int32_t TimerTask::GetEndTime()
    {
        if (end_timestamp_ < Timestamp::now() )
        {
            return 0;
        }
        return (int32_t)timer_id_.GetTimer()->expiration().secondsSinceEpoch();
    }

    void TimerTask::UpdateEndTeamDate()
    {
        if (nullptr != timer_id_.GetTimer())
        {
            end_timestamp_ = timer_id_.GetTimer()->expiration();
        }
    }

    void TimerTask::SetCallBack(const TimerCallback& cb)
    {
        timer_function_callback_ = cb;
    }

    void TimerTask::OnTimer()
    {
        if (!timer_function_callback_)
        {
            return;
        }
        TimerCallback copycb = timer_function_callback_;
        copycb();
        end_timestamp_ = Timestamp();
    }
}// namespace common